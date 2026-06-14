#include "applicationclass.h"
#include <algorithm>
#include <cctype>
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
#include <stb_image.h>
#include "../../CharacterSpawnSystem.h"
#include "../../CharacterMovementSystem.h"
#include "../../CharacterControllerSystem.h"
#include "../../AnimationController.h"
#include "../../CharacterFacingSystem.h"
#include "../Render/Grass/GrassRenderer.h"
#include "../../WorldAreaComponent.h"

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

    std::vector<float> rgba(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    for (int i = 0; i < width * height; ++i)
    {
        rgba[i * 4 + 0] = hdrData[i * 3 + 0];
        rgba[i * 4 + 1] = hdrData[i * 3 + 1];
        rgba[i * 4 + 2] = hdrData[i * 3 + 2];
        rgba[i * 4 + 3] = 1.0f;
    }

    // Создаём 2D HDR texture. R32G32B32A32 is broadly supported by D3D11 SRVs.
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = rgba.data();
    initData.SysMemPitch = width * 4 * sizeof(float);

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
    return srv;
}

bool ApplicationClass::ReloadSkyBox(std::string newSkyboxPath)
{
    ID3D11ShaderResourceView* newSkySRV = LoadEquirectangularHDR(
        m_direct3D->GetDevice(),
        m_direct3D->GetDeviceContext(),
        newSkyboxPath);

    if (!newSkySRV)
    {
        OutputDebugStringA(("ReloadSkyBox failed: " + newSkyboxPath + "\n").c_str());
        return false;
    }

    if (m_skySRV)
    {
        m_skySRV->Release();
        m_skySRV = nullptr;
    }

    m_skySRV = newSkySRV;

    if (m_SkyShader)
        m_SkyShader->SetSkyTexture(m_skySRV);

    if (m_pbrShader)
        m_pbrShader->SetEnvironmentMap(m_skySRV);

    return true;
}


ID3D11ShaderResourceView* ApplicationClass::LoadSkyboxFromWorldSettings(
    ID3D11Device* device,
    ID3D11DeviceContext* context)
{
    const auto& world = m_sceneManager.GetWorldSettings();

    if (auto* srv = LoadEquirectangularHDR(
        device,
        context,
        world.skyboxPath);
        srv)
    {
        return srv;
    }

    OutputDebugStringA(("Failed to load HDR skybox from WorldSettings: " + world.skyboxPath + "\n").c_str());
    return nullptr;
}



// ============================================================================
// Initialize
// ============================================================================
bool ApplicationClass::Initialize(int screenWidth, int screenHeight, HWND hwnd)
{
    m_hwnd = hwnd;
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;

    // -- Direct3D -------------------------------------------------------------
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
        rsDesc.FillMode = D3D11_FILL_WIREFRAME;  // < КЛЮЧЕВОЙ ПАРАМЕТР
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

    // -- Файловая система -----------------------------------------------------
    m_gameFs = std::make_unique<FileSystem>(fs::current_path() / "assets");
    m_gameFs->ScanFileSystem();

    // -- EditorUI -------------------------------------------------------------
    if (!m_editorUI.Initialize(hwnd, m_direct3D->GetDevice(), m_direct3D->GetDeviceContext(), m_gameFs.get()))
        return false;

    // -- Камера ---------------------------------------------------------------
    m_camera = std::make_unique<CameraClass>();
    if (!m_camera) return false;
    m_camera->SetPosition(0.0f, 2.0f, -10.0f);
    m_camera->SetRotation(0.0f, 0.0f, 0.0f);
    m_cameraController.SetCamera(m_camera.get());
    m_cameraController.SetInput(&m_inputManager, m_input, m_hwnd);

    // -- Fallback модель (для SpawnObject без указания модели) -----------------
 
    // -- Модель для сфер-индикаторов света ------------------------------------
    m_lightSphereModel = new AssimpModelClass;
    if (!m_lightSphereModel->Initialize(m_direct3D->GetDevice(), m_direct3D->GetDeviceContext(),
        "assets/model/sphere.obj", "assets/texture/stone.tga"))
    {
        MessageBox(hwnd, L"Failed to load light sphere model", L"Error", MB_OK);
        return false;
    }

    m_sceneManager.SetLightModel(m_lightSphereModel);
    m_sceneManager.SetFallbackModel(m_lightSphereModel);
    m_sceneManager.SetAssetLoadSystem(&m_assetLoadSystem);
  


    // -- Загрузка всех шейдеров -----------------------------------------------
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
    m_terrainBlendShader = sm.Load<TerrainBlendShaderClass>("terrain_blend", m_direct3D->GetDevice(), hwnd);
    m_SkyShader = sm.Load<SkyShaderClass>("sky",m_direct3D->GetDevice(), hwnd);
  // m_fontShader = sm.Load<FontShaderClass>("font", m_direct3D->GetDevice(), hwnd);
    

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

    if (!m_colorShader->Initialize(m_direct3D->GetDevice(), hwnd))
    {
        MessageBox(hwnd, L"Failed to initialize ColorShader", L"Error", MB_OK);
        return false;
    }

    if (!m_areaDebugRenderer.Initialize(m_direct3D->GetDevice()))
    {
        MessageBox(hwnd, L"Failed to initialize AreaDebugRenderer", L"Error", MB_OK);
        return false;
    }
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

    // ==========================================================
    // SHADOW SHADER
    // ==========================================================
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

    // ?? FIX: Расширяем ортогональную проекцию (была 30x30, теперь 50x50)
    // Это позволит охватить все объекты
    XMMATRIX lightProj = XMMatrixOrthographicLH(100, 100, 0.5f, 200.0f);

    m_shadowMap->SetLightViewProjection(lightView, lightProj);

    m_shadowMap->SetLightViewProjection(lightView, lightProj);
    // Создаём тестовую физическую сцену
   // CreatePhysicsTestScene();



    // Создаём визуализатор отладочных контактов
    m_debugVisualizer = new DebugPhysicsVisualizer();


    if (!m_textureShader || !m_lightShader || !m_pbrShader || !m_terrainBlendShader ||
        !m_multiTextureShader || !m_lightMapShader || !m_normalMapShader)
    {
        OutputDebugStringA("One or more critical shaders failed to load. Initialize() abort.\n");
        return false;
    }
    // -- Render-to-texture для отражения и преломления -------------------------
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

    // -- Сферы-индикаторы для источников света --------------------------------
    m_sceneManager.SetupLightSpheres(m_lightSpheres, m_lightSphereModel);

    // -- Один источник света по умолчанию -------------------------------------


    // -- Default material для всех объектов без ручного назначения ---------
    // Раньше было: создавали Material, но не инициализировали/не назначали.
    // Из-за этого у объектов shader оставался nullptr, и модель "не видна",
    // пока не сменишь shader в UI (SetObjectShader создаёт material).
    m_defaultMaterial = new MaterialClass();
    if (!m_defaultMaterial->Initialize(m_direct3D->GetDevice(), L"DefaultMaterial"))
    {
        OutputDebugStringA("Failed to initialize m_defaultMaterial\n");
        return false;
    }

    // World geometry should react to the sun and receive shadows by default.
    // TextureShader stays available as an explicit unlit mode.
    m_defaultMaterial->SetShader(m_lightShader);

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

    // -- Модель для капсул (опционально) -----------------------------
    m_capsuleModel = new AssimpModelClass;
    if (!m_capsuleModel->Initialize(m_direct3D->GetDevice(), m_direct3D->GetDeviceContext(),
        "assets/model/capsule.obj", "assets/texture/wireframe.tga"))
    {
        OutputDebugStringA("Warning: Failed to load capsule.obj, capsule debug disabled\n");
        delete m_capsuleModel;
        m_capsuleModel = nullptr;
    }

    // -- Render-to-texture для Scene Preview ----------------------------------
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

    m_grassRenderer = new GrassRenderer();
    if (m_grassRenderer->Initialize(
        m_direct3D->GetDevice(),
        m_direct3D->GetDeviceContext()))
    {
        m_renderer->SetGrassRenderer(m_grassRenderer);
    }
    else
    {
        OutputDebugStringA("Warning: GrassRenderer prototype failed to initialize\n");
        delete m_grassRenderer;
        m_grassRenderer = nullptr;
    }

   
    m_dragDropSystem = std::make_unique<DragDropSystem>(
        m_camera.get(), &m_inputManager, m_input, &m_editorUI, &m_sceneManager);

    m_skySRV = LoadSkyboxFromWorldSettings(
        m_direct3D->GetDevice(),
        m_direct3D->GetDeviceContext());

    if (!m_skySRV)
    {
        OutputDebugStringA("?? Warning: Failed to load skybox, using fallback\n");
        // Можно создать заглушку или продолжить без неба
    }
    else
    {
    }

    // Передаём в рендерер
    if (m_renderer && m_SkyShader)
    {
        m_renderer->SetSkyShader(m_SkyShader);
        m_SkyShader->SetSkyTexture(m_skySRV);  // < Новый метод в шейдере
    }

   

  
 

    // ── Текст "Hello" ───────────────────────────────────────────────────────
  /*  char helloText[32] = "Hello";

    m_textHello = new TextClass();
    if (!m_textHello->Initialize(
        m_direct3D->GetDevice(),
        m_direct3D->GetDeviceContext(),
       m_screenWidth,
       m_screenHeight, 10, m_font))
       
    {
        MessageBox(hwnd, L"Failed to initialize TextClass 'Hello'", L"Error", MB_OK);
        return false;
    }*/
   
   // m_textHello->UpdateText(helloText, 960, 540, 0, 0, 0);

	m_jobSystem.Initialize(4); // Инициализируем с 4 рабочими потоками (можно по количеству ядер)


	m_sceneManager.SpawnSprite(m_direct3D->GetDevice(), m_direct3D->GetDeviceContext(), "assets/texture/sprite_test.png", 0, 0, 0, 1, 1);


    return true;
}






// ============================================================================
// Shutdown
// ============================================================================
void ApplicationClass::Shutdown()
{
    m_jobSystem.Shutdown();
    m_jobSystem.ExecuteMainThreadJobs();

    m_areaDebugRenderer.Shutdown();

    if (m_renderer)
        m_renderer->SetGrassRenderer(nullptr);

    if (m_grassRenderer)
    {
        m_grassRenderer->Shutdown();
        delete m_grassRenderer;
        m_grassRenderer = nullptr;
    }

    if (m_direct3D && m_direct3D->GetDeviceContext())
    {
        m_direct3D->GetDeviceContext()->ClearState();
        m_direct3D->GetDeviceContext()->Flush();
    }

    // Bullet keeps non-owning pointers to ECS physics components. Detach those
    // registrations before clearing the registry and deleting the components.
    PhysicsWorld::Get().Shutdown();
    m_sceneManager.Clear();

    if (m_lightSphereModel)
    {
        m_lightSphereModel->Shutdown();
        delete m_lightSphereModel;
        m_lightSphereModel = nullptr;
    }

    ShaderManager::Get().Shutdown();

    m_assetLoadSystem.Shutdown();

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

    m_editorUI.Shutdown();

    if (m_skySRV)
    {
        m_skySRV->Release();
        m_skySRV = nullptr;
    }

    if (m_wireframeRS)
    {
        m_wireframeRS->Release();
        m_wireframeRS = nullptr;
    }

    if (m_direct3D)
        m_direct3D->Shutdown();

}

bool ApplicationClass::Frame(float dt)
{
    // =====================================================
    // 0. Input
    // =====================================================
    m_inputManager.Update(m_input);

    
    // =====================================================
    // 1. Engine mode toggle: F5 = Editor / Play
    // =====================================================
    const bool f5Down = m_input && m_input->IsKeyDown(VK_F5);
    const bool f1Down = m_input && m_input->IsKeyDown(VK_F1);

    if (f1Down && !m_wasF1Down)
    {
        m_editorUI.ToggleObjectDebugWindow();
    }
    m_wasF1Down = f1Down;

   if (f5Down && !m_wasF5Down)
{
    if (m_engineMode == EngineMode::Editor)
    {
        if (m_gameplayController.EnterPlayMode(m_sceneManager.GetECSRegistry()))
        {
            m_engineMode = EngineMode::Play;
            m_cameraController.EnterPlayMode();
        }
    }
    else
    {
        m_engineMode = EngineMode::Editor;
        m_cameraController.ExitPlayMode();
    }
}

    m_wasF5Down = f5Down;

    const bool isEditorMode = m_engineMode == EngineMode::Editor;
    const bool isPlayMode = m_engineMode == EngineMode::Play;

    m_jobSystem.ExecuteMainThreadJobs();

    m_assetLoadSystem.Update(m_direct3D->GetDevice());

    // =====================================================
    // 2. Editor input / camera
    // =====================================================
    bool imguiWantsMouse = false;

    if (isEditorMode)
    {
        m_physicsDebugSettings = m_editorUI.GetPhysicsDebugSettings();

        imguiWantsMouse =
            m_editorUI.WantCaptureMouse()
            || ImGuizmo::IsOver()
            || ImGuizmo::IsUsing();

        m_cameraController.UpdateModeToggle(imguiWantsMouse);

        if (m_cameraController.IsFreeMode())
        {
            m_cameraController.UpdateFree(dt, imguiWantsMouse);
        }
        else if (imguiWantsMouse)
        {
            m_cameraController.ReleaseMouseCapture();
        }
    
    }
    

    // =====================================================
    // 3. Gameplay input -> controller intent
    // =====================================================
    if (isPlayMode)
    {
        m_gameplayController.Update(
            m_sceneManager.GetECSRegistry(),
            dt,
            m_input,
            false,
            m_cameraController.GetFollowYaw()
        );
        CharacterFacingSystem::Update(m_sceneManager.GetECSRegistry(), dt);
        CharacterControllerSystem::Update(m_sceneManager.GetECSRegistry(), dt);
        AnimationController::Update(m_sceneManager.GetECSRegistry(), dt);
        
       
    }

    // =====================================================
    // 4. World systems / physics
    // =====================================================
    SyncCompartmentPositions();
    UpdatePhysics(dt);
    UpdateWaterAnimation(dt);

    // =====================================================
    // 5. Play camera follows post-physics transform
    // =====================================================
    if (isPlayMode)
    {
        auto& registry = m_sceneManager.GetECSRegistry();

        Entity player = INVALID_ENTITY;

        registry.ForEach<PlayerGameTag>(
            [&](Entity entity, PlayerGameTag& tag)
            {
                player = entity;
            });

        if (player != INVALID_ENTITY)
        {
            auto* transform =
                registry.GetComponent<TransformComponent>(player);

            if (transform)
            {
                m_cameraController.UpdateFollow(dt, transform);
            }
        }
    }

    // =====================================================
    // 6. Scene update
    // =====================================================
    m_sceneManager.UpdateAll(dt);
    m_sceneManager.UpdateLights();

    m_camera->Render();
    m_camera->GetViewMatrix(m_viewMatrix);
    m_direct3D->GetProjectionMatrix(m_projMatrix);

    // =====================================================
    // 6. Debug visualizer
    // =====================================================
    if (m_debugVisualizer)
    {
        m_debugVisualizer->UpdateFromWorld(
            PhysicsWorld::Get().GetWorld()
        );
    }

    // =====================================================
    // 7. Editor-only tools
    // =====================================================
    if (isEditorMode)
    {
        imguiWantsMouse =
            m_editorUI.WantCaptureMouse()
            || ImGuizmo::IsOver()
            || ImGuizmo::IsUsing();

        if (!imguiWantsMouse && m_dragDropSystem)
        {
            m_dragDropSystem->Update(
                m_viewMatrix,
                m_projMatrix,
                m_screenWidth,
                m_screenHeight,
                m_lightSpheres,
                [this](const auto& req)
                {
                    SpawnObjectFromRequest(req);
                });
        }
    }
    // =====================================================
    // 8. Render 3D scene
    // =====================================================
    Entity selectedEntity = INVALID_ENTITY;

    if (isEditorMode &&
        m_editorUI.GetSelectedType() == EditorUI::SelectedType::Object)
    {
        selectedEntity = m_editorUI.GetSelectedEntity();
    }

    m_renderer->RenderFrame(
        m_direct3D->GetDeviceContext(),
        m_viewMatrix,
        m_projMatrix,
        selectedEntity,
        m_physicsDebugSettings
    );

    auto& areas =
        m_sceneManager.GetECSRegistry()
        .GetComponents<WorldAreaComponent>();

    for (auto& [entity, area] : areas)
    {
        if (!area.enabled || !m_sceneManager.IsObjectActive(entity))
            continue;

        m_areaDebugRenderer.Render(
            m_direct3D->GetDeviceContext(),
            area,
            m_viewMatrix,
            m_projMatrix,
            m_colorShader);
    }


    // =====================================================
    // 9. Editor UI
    // =====================================================
    if (isEditorMode)
    {
        m_editorUI.Update(m_sceneManager);

        RenderCompartmentCornerHandles();
        RenderCompoundCornerHandles();
        HandleCompartmentGizmo();

        ProcessEditorRequests();

        m_editorUI.Render();

        if (m_sceneManager.GetWorldSettings().skyboxReloadPending)
        {
            if (ReloadSkyBox(m_sceneManager.GetWorldSettings().skyboxPath))
            {
            }
            else
            {
                OutputDebugStringA(("Failed to reload skybox from WorldSettings: " + m_sceneManager.GetWorldSettings().skyboxPath + "\n").c_str());
            }
            m_sceneManager.GetWorldSettings().skyboxReloadPending = false;
        }
    }
    else if (m_editorUI.IsObjectDebugWindowVisible())
    {
        m_editorUI.UpdateDebugOverlay(m_sceneManager);
        m_editorUI.Render();
    }


    {
        XMMATRIX worldMatrix, viewMatrix, orthoMatrix;

        worldMatrix = XMMatrixIdentity();
        viewMatrix = XMMatrixIdentity();
        orthoMatrix = XMMatrixOrthographicLH(
            static_cast<float>(m_screenWidth),
            static_cast<float>(m_screenHeight),
            0.0f,
            1.0f);

        // Отключаем Z-буфер и включаем альфа-смешивание для 2D-текста
        m_direct3D->TurnZBufferOff();
        m_direct3D->TurnAlphaBlendingOn();

       

        // Возвращаем состояние для 3D
        m_direct3D->TurnZBufferOn();
        m_direct3D->TurnAlphaBlendingOff();
    }

    // =====================================================
    // 10. Present
    // =====================================================
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

    // -- Находим мировую матрицу корневой подлодки -----------------------------
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
            {
                m_sceneManager.SaveScene("Level.json");
            }
            else
            {
                OutputDebugStringA(("Failed to save scene to " + req.path + "\n").c_str());
            }

        }
    }

    if (m_editorUI.ConsumeRebuildGrassRequest() && m_grassRenderer)
    {
        auto& areas = m_sceneManager
            .GetECSRegistry()
            .GetComponents<WorldAreaComponent>();

        m_grassRenderer->GenerateAreas(
            m_direct3D->GetDevice(),
            areas);
    }

    // Load request
    {
        auto req = m_editorUI.ConsumeLoadRequest();
        if (req.pending)
        {
            m_sceneManager.LoadScene(req.path,
                m_direct3D->GetDevice(), m_direct3D->GetDeviceContext());
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
    ID3D11Device* device = m_direct3D->GetDevice();
    ID3D11DeviceContext* context = m_direct3D->GetDeviceContext();

    std::string modelPathLower = req.modelPath;
    std::transform(modelPathLower.begin(), modelPathLower.end(),
        modelPathLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    const bool isGlb = modelPathLower.ends_with(".glb") || modelPathLower.ends_with(".gltf");
    const bool useAsyncMesh = !isGlb && !req.modelPath.empty();

    AssimpModelClass* model = nullptr;
    if (useAsyncMesh)
    {
        model = m_sceneManager.GetLightModel();
        if (!model)
            model = m_lightSphereModel;

        m_assetLoadSystem.RequestMeshLoad(req.modelPath);
    }
    else
    {

        model = m_sceneManager.GetOrCreateModel(
            req.modelPath, device, context, req.texturePath);
    }

    if (!model)
    {
        OutputDebugStringA("SpawnObjectFromRequest: model FAILED\n");
        return;
    }
    AssimpModelClass* collisionModel = nullptr;

    if (req.useSeparateCollisionMesh)
    {
        collisionModel = m_sceneManager.GetOrCreateModel(
            req.collisionModelPath,
            device,
            context,
            "", // Физическому мешу текстура не нужна
            false
        );

        if (!collisionModel)
        {
            OutputDebugStringA(
                ("Collision mesh not found: " +
                    req.collisionModelPath + "\n").c_str()
            );

            return;
        }
    }

    auto* mat = new MaterialClass();
    if (!mat->Initialize(device, L"SpawnedMaterial"))
    {
        delete mat;
        return;
    }

    ShaderBaseClass* initShader = isGlb
        ? static_cast<ShaderBaseClass*>(m_pbrShader)
        : static_cast<ShaderBaseClass*>(m_lightShader);
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
        }
        else
        {
            delete tex;
            if (!isGlb && m_defaultTexture)
                mat->SetMainTexture(m_defaultTexture);
        }
    }
    else if (!isGlb && m_defaultTexture)
    {
        mat->SetMainTexture(m_defaultTexture);
    }

    Entity newEntity = m_sceneManager.SpawnObject(model, req.posX, req.posY, req.posZ);

    if (req.useSeparateCollisionMesh)
    {
        auto& registry = m_sceneManager.GetECSRegistry();

        RigidBodyComponent rb;
        rb.collisionModel = collisionModel;
        rb.collisionModelPath = req.collisionModelPath;
        rb.useSeparateCollisionMesh = true;

        // Для террайна:
        rb.shapeType = RigidBodyComponent::TriangleMesh;
        rb.mass = 0.0f;
        rb.isDynamic = false;

        registry.AddComponent<RigidBodyComponent>(
            newEntity,
            std::move(rb)
        );

        PhysicsSystem::CreateRigidBody(registry, newEntity);
    }
    m_sceneManager.SetObjectMaterial(newEntity, mat);

    if (useAsyncMesh)
    {
        if (auto* mesh = m_sceneManager.GetObjectMesh(newEntity))
        {
            mesh->asyncModelPath = req.modelPath;
            mesh->useAsyncMesh = true;
        }
    }

    m_editorUI.SetSelection(EditorUI::SelectedType::Object, newEntity);
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
    if (!m_pbrShader && m_defaultTexture) subMat->SetMainTexture(m_defaultTexture);

    registry.AddComponent<MeshRendererComponent>(subRoot,
        MeshRendererComponent{
            .model = subModel,
            .material = subMat
        });
    registry.AddComponent<SubmarineComponent>(subRoot, SubmarineComponent{ subRoot });
    registry.AddComponent<ActiveComponent>(subRoot, ActiveComponent{});

    // ==================================================================
    // КОЛЛИЗИЯ ПОДЛОДКИ: Compound из 3 частей (нос + тело + корма)
    // Каждая часть — capsule/cylinder, вместе дают форму "пули"
    // ==================================================================
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

    // ==================================================================
    // ОТСЕКИ: только логика + СТЕНЫ для персонажей
    // ==================================================================
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

    // -- Находим мировую матрицу корневой подлодки -----------------------------
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

    // -- 8 углов AABB ----------------------------------------------------------
    // Биты индекса: бит0=X(0>min,1>max), бит1=Y, бит2=Z
    auto getCornerLocal = [&](int idx) -> XMFLOAT3 {
        return {
            (idx & 1) ? comp->localMax.x : comp->localMin.x,
            (idx & 2) ? comp->localMax.y : comp->localMin.y,
            (idx & 4) ? comp->localMax.z : comp->localMin.z
        };
        };

    // Проецирует локальную точку отсека > пиксели экрана
    auto projectPoint = [&](const XMFLOAT3& localPt) -> ImVec2 {
        XMVECTOR v = XMLoadFloat3(&localPt);
        XMVECTOR w = XMVector3TransformCoord(v, rootWorld);
        XMVECTOR clip = XMVector3TransformCoord(w, vp);
        XMFLOAT3 ndc;
        XMStoreFloat3(&ndc, clip);
        return ImVec2((ndc.x * 0.5f + 0.5f) * W,
            (1.0f - (ndc.y * 0.5f + 0.5f)) * H);
        };

    // -- Рисуем рёбра AABB (12 линий) -----------------------------------------
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

    // -- Ручки углов -----------------------------------------------------------
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

    // -- Текущие границы бокса -------------------------------------------------
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

    // -- 8 угловых точек -------------------------------------------------------
    auto getCorner = [&](int idx) -> XMFLOAT3 {
        return {
            (idx & 1) ? bMax.x : bMin.x,
            (idx & 2) ? bMax.y : bMin.y,
            (idx & 4) ? bMax.z : bMin.z
        };
        };

    // -- 6 граневых точек (центр каждой грани) — двигают ТОЛЬКО одну ось -------
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

    // -- Рёбра AABB ------------------------------------------------------------
    static constexpr int edges[12][2] = {
        {0,1},{2,3},{4,5},{6,7},
        {0,2},{1,3},{4,6},{5,7},
        {0,4},{1,5},{2,6},{3,7}
    };
    for (auto& [a, b] : edges)
        dl->AddLine(projectPt(getCorner(a)), projectPt(getCorner(b)),
            IM_COL32(255, 160, 30, 180), 1.5f);

    // -- Завершаем drag по отпусканию ЛКМ -------------------------------------
    if (m_editorUI.m_cornerDragActive && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        m_editorUI.m_cornerDragActive = false;
        m_editorUI.m_cornerDragIndex = -1;
        RefreshRigidBodyExternal(registry, entity, rb);
    }

    // -- Обновляем drag --------------------------------------------------------
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
                // -- Угловая точка: меняем все 3 оси одновременно -------------
                if (idx & 1) newMax.x = fmaxf(hitOffset.x, newMin.x + MIN_SIZE);
                else         newMin.x = fminf(hitOffset.x, newMax.x - MIN_SIZE);
                if (idx & 2) newMax.y = fmaxf(hitOffset.y, newMin.y + MIN_SIZE);
                else         newMin.y = fminf(hitOffset.y, newMax.y - MIN_SIZE);
                if (idx & 4) newMax.z = fmaxf(hitOffset.z, newMin.z + MIN_SIZE);
                else         newMin.z = fminf(hitOffset.z, newMax.z - MIN_SIZE);
            }
            else
            {
                // -- Граневая точка: меняем ТОЛЬКО одну ось -------------------
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

    // -- Рисуем ручки ---------------------------------------------------------
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

void ApplicationClass::UpdateCameraControlMode()
{
    bool imguiWantsMouse =
        m_editorUI.WantCaptureMouse() ||
        ImGuizmo::IsOver() ||
        ImGuizmo::IsUsing();

    m_cameraController.UpdateModeToggle(imguiWantsMouse);

    if (imguiWantsMouse)
    {
        m_cameraController.ReleaseMouseCapture();
    }
}
