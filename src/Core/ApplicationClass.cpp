#include "applicationclass.h"
#include <algorithm>
#include "FileSystem.h"
#include "PhysicsSystem.h"
#include "helper.h"
#include "TimeManager.h"
#include "PressureSystem.h"
#include "SubmarineJointSystem.h"
#include "FloodingSystem.h"
#include "SubmarineBuilder.h"
#include "JointConnectionSystem.h"
#include "../../SubmarineSystem.h"

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

    CreateTestSubmarine();

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

    // Устанавливаем проекционную текстуру (слот 2)
    TextureClass* projTex = new TextureClass();
    projTex->Initialize(m_direct3D->GetDevice(), m_direct3D->GetDeviceContext(), "assets/texture/projection.tga");
    mat->SetTexture(2, projTex);

    // Задаём матрицы второй камеры (вид от проектора)
    XMMATRIX view2 = XMMatrixLookAtLH({ -5,5,-5 }, { 0,0,0 }, { 0,1,0 });
    XMMATRIX proj2 = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), 1.0f, 0.1f, 100.0f);
    mat->SetProjectionView2(view2);
    mat->SetProjectionProj2(proj2);

    
    Entity dx = m_sceneManager.SpawnLight({ 0, 0, 0 }, { 1, 1, 1, 1 });



    ////////////////////////////////////////////////\\
    








  
    return true;
}



void ApplicationClass::CreatePhysicsTestScene()
{
    auto& registry = m_sceneManager.GetECSRegistry();

    // Пол
    Entity floor = m_sceneManager.SpawnObject(m_mainModel, 0, -2, 0);
    registry.AddComponent<RigidBodyComponent>(floor, RigidBodyComponent{});
    RigidBodyComponent* floorRb = registry.GetComponent<RigidBodyComponent>(floor);
    if (floorRb)
    {
        floorRb->mass = 0.0f;
        floorRb->shapeType = RigidBodyComponent::Plane;
        floorRb->shapeSize = btVector3(10, 0.5f, 10);
        PhysicsSystem::CreateRigidBody(registry, floor);
    }

    // Падающая сфера
    Entity sphere = m_sceneManager.SpawnObject(m_lightSphereModel, 0, 3, 0);
    registry.AddComponent<RigidBodyComponent>(sphere, RigidBodyComponent{});
    RigidBodyComponent* sphereRb = registry.GetComponent<RigidBodyComponent>(sphere);
    if (sphereRb)
    {
        sphereRb->mass = 1.0f;
        sphereRb->shapeType = RigidBodyComponent::Sphere;
        sphereRb->shapeSize = btVector3(0.5f, 0, 0);
        PhysicsSystem::CreateRigidBody(registry, sphere);
    }

    // Лог количества тел в мире
    int numBodies = PhysicsWorld::Get().GetWorld()->getNumCollisionObjects();
    char buf[256];
    sprintf_s(buf, "Num bodies after creation: %d\n", numBodies);
    OutputDebugStringA(buf);
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

    if (m_direct3D)
        m_direct3D->Shutdown();

    m_editorUI.Shutdown();
}

// ============================================================================
// Frame
// ============================================================================
bool ApplicationClass::Frame(float dt)
{
    // Очищаем линии от предыдущего кадра
    m_physicsDebugSettings = m_editorUI.GetPhysicsDebugSettings();

    {
        auto& registry = m_sceneManager.GetECSRegistry();
        auto& compartmentPool = registry.GetComponents<CompartmentComponent>();
        auto& transformPool = registry.GetComponents<TransformComponent>();
        auto& subPool = registry.GetComponents<SubmarineComponent>();

        for (auto& [entity, comp] : compartmentPool)
        {
            auto* subLink = subPool.Get(entity);
            if (!subLink || subLink->submarineRoot == INVALID_ENTITY) continue;
            if (entity == subLink->submarineRoot) continue;  // ← Пропускаем корень!

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

            // Прибавляем к позиции подлодки
            XMVECTOR rootPos = XMLoadFloat3(&rootTransform->position);
            XMVECTOR worldPos = XMVectorAdd(rootPos, rotatedOffset);

            // Обновляем трансформ отсека
            XMFLOAT3 newPos;
            XMStoreFloat3(&newPos, worldPos);
            compartmentTransform->position = newPos;
            compartmentTransform->rotation = rootTransform->rotation;
        }
    }
     // Построение визуализации коллизий
  // Вариант 1: Простое исправление
    if (dt > 0.0f)
    {
        auto& registry = m_sceneManager.GetECSRegistry();
        auto& subPool = registry.GetComponents<SubmarineComponent>();
        auto& transformPool = registry.GetComponents<TransformComponent>();
        // 1. Затопление отсеков (накопление воды через пробоины)
        FloodingSystem::Update(registry, dt, m_sceneManager.GetWaterLevel());

        // ═══════════════════════════════════════════════════════════════
        // 🔥 FIX: Вызываем JointConnectionSystem ПЕРЕД физикой.
        //    Этот вызов ОТСУТСТВОВАЛ — джойнты из редактора
        //    никогда не создавались!
        // ═══════════════════════════════════════════════════════════════
        JointConnectionSystem::Update(registry);



        for (auto& [entity, subComp] : subPool)
        {
            if (subComp.submarineRoot == entity)  // Только корневые
            {
                SubmarineSystem::UpdateSubmarine(registry, entity);
            }
        }

        // 2. Физика (плавучесть + Bullet step + обратная синхронизация)
        PhysicsSystem::Update(registry, dt, m_sceneManager.GetWaterLevel());

        // ═══════════════════════════════════════════════════════════════
        // 🔥 FIX: Вызываем SubmarineJointSystem ПОСЛЕ физики.
        //    Этот вызов ОТСУТСТВОВАЛ — сломанные джойнты никогда
        //    не обнаруживались и не помечались!
        // ═══════════════════════════════════════════════════════════════
        SubmarineJointSystem::Update(registry, dt);

        // 3. Давление/ремонтные системы
        PressureSystem::Update(registry, dt, m_sceneManager.GetWaterLevel());
    }

    if (m_lightShader)
    {
        // Направление света (вниз и немного вбок)
        XMFLOAT3 sunDirection = XMFLOAT3(0.3f, -1.0f, 0.5f);

        // Цвет солнца (тёплый жёлтый)
        XMFLOAT4 sunColor = XMFLOAT4(1.0f, 0.95f, 0.8f, 1.0f);

        // Ambient освещение (голубоватое небо)
        XMFLOAT4 ambientColor = XMFLOAT4(0.15f, 0.2f, 0.3f, 0.25f); // a = сила

        // Интенсивность солнца
        float sunIntensity = 1.0f;

        m_lightShader->SetDirectionalLight(
            m_direct3D->GetDeviceContext(),
            sunDirection,
            sunColor,
            ambientColor,
            sunIntensity
        );
    }


    {
        auto& meshPool = m_sceneManager.GetECSRegistry()
            .GetComponents<MeshRendererComponent>();

        OutputDebugStringA("\n╔════════════════════════════════════════╗\n");
        OutputDebugStringA("║  PRE-SHADOW MAP OBJECTS                 ║\n");
        OutputDebugStringA("╚════════════════════════════════════════╝\n");

        char buf[256];
        sprintf_s(buf, "Total meshes: %zu\n", meshPool.GetCount());
        OutputDebugStringA(buf);

        for (auto& [ent, mesh] : meshPool)
        {
            if (!m_sceneManager.IsObjectActive(ent)) continue;

            auto* transform = m_sceneManager.GetObjectTransform(ent);
            auto* mat = mesh.material;

            if (!transform || !mesh.model) continue;
            if (!mat || !mat->GetShader()) continue;

            ShaderType st = mat->GetShader()->GetType();

            sprintf_s(buf, "  entity=%u, pos=(%.1f,%.1f,%.1f), shader=%s\n",
                ent, transform->position.x, transform->position.y, transform->position.z,
                (st == ShaderType::Light) ? "Light" : (st == ShaderType::PBR) ? "PBR" : "Other");
            OutputDebugStringA(buf);
        }
    }

    m_shadowMap->SetRenderTarget(m_direct3D->GetDeviceContext());
    m_shadowMap->ClearRenderTarget(m_direct3D->GetDeviceContext());

    XMMATRIX lightVP = m_shadowMap->GetLightViewProjection();
    Render3DObjectsToShadowMap(m_direct3D->GetDeviceContext(), lightVP);

    // Восстанавливаем backbuffer
    m_direct3D->SetBackBufferRenderTarget();
    m_direct3D->ResetViewport();

    // 🔥 ДОБАВИТЬ
    if (m_pbrShader)
    {

        XMFLOAT3 sunDirection = XMFLOAT3(0.3f, -1.0f, 0.5f);

        // Цвет солнца (тёплый жёлтый)
        XMFLOAT4 sunColor = XMFLOAT4(1.0f, 0.95f, 0.8f, 1.0f);

        // Ambient освещение (голубоватое небо)
        XMFLOAT4 ambientColor = XMFLOAT4(0.15f, 0.2f, 0.3f, 0.25f); // a = сила

        float sunIntensity = 1.0f;

        m_pbrShader->SetDirectionalLight(
            m_direct3D->GetDeviceContext(),
            sunDirection, sunColor, ambientColor, sunIntensity);
    }
    m_inputManager.Update(m_input);
    HandleCameraMovement(TimeManager::UnscaledDeltaTime());
    m_sceneManager.UpdateAll(dt);


  

    // Рендерим в shadow map (без текстур, только depth)
   
  



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

    // ВАЖНО: обновляем контакты до рендеринга — иначе Render() рисует по старым/пустым данным.
    if (m_debugVisualizer)
    {
        m_debugVisualizer->UpdateFromWorld(PhysicsWorld::Get().GetWorld());
    }

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
    m_reflectionTexture->SetRenderTarget(m_direct3D->GetDeviceContext());
    m_reflectionTexture->ClearRenderTarget(m_direct3D->GetDeviceContext(), 0.0f, 0.0f, 0.0f, 1.0f);
    Render3DObjectsExcluding(m_direct3D->GetDeviceContext(), reflectionMatrix, m_projMatrix, -1);
    m_direct3D->SetBackBufferRenderTarget();
    m_direct3D->ResetViewport();
    {
        ID3D11ShaderResourceView* nullSRVs[8] = {};
        m_direct3D->GetDeviceContext()->PSSetShaderResources(0, 8, nullSRVs);
    }

    // ── Pass 1: рендер ПРЕЛОМЛЕНИЯ → m_refractionTexture ─────────────────────
    m_refractionTexture->SetRenderTarget(m_direct3D->GetDeviceContext());
    m_refractionTexture->ClearRenderTarget(m_direct3D->GetDeviceContext(), 0.0f, 0.0f, 0.0f, 1.0f);
    {
        auto& meshPool = m_sceneManager.GetECSRegistry().GetComponents<MeshRendererComponent>();
        XMFLOAT4 lightColors[NUM_LIGHTS];
        XMFLOAT4 lightPositions[NUM_LIGHTS];
    

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
            RenderMesh(m_direct3D->GetDeviceContext(), mesh.model, world, m_viewMatrix, m_projMatrix,
                mat, lightColors, lightPositions,
                m_sceneManager.GetFogSettings(), false);
        }
    }
    m_direct3D->SetBackBufferRenderTarget();
    m_direct3D->ResetViewport();
    {
        ID3D11ShaderResourceView* nullSRVs[8] = {};
        m_direct3D->GetDeviceContext()->PSSetShaderResources(0, 8, nullSRVs);
    }

    // ── Pass 2: preview в RenderTexture (для EditorUI) ────────────────────────
    m_renderTexture->SetRenderTarget(m_direct3D->GetDeviceContext());
    m_renderTexture->ClearRenderTarget(m_direct3D->GetDeviceContext(),
        m_sceneManager.GetFogSettings().color.x,
        m_sceneManager.GetFogSettings().color.y,
        m_sceneManager.GetFogSettings().color.z, 1.0f);
    XMMATRIX rtProj;
    m_renderTexture->GetProjectionMatrix(rtProj);
    Render3DObjects(m_direct3D->GetDeviceContext(), m_viewMatrix, rtProj);
    m_editorUI.SetScenePreviewTexture(m_renderTexture->GetShaderResourceView());
    {
        ID3D11ShaderResourceView* nullSRVs[8] = {};
        m_direct3D->GetDeviceContext()->PSSetShaderResources(0, 8, nullSRVs);
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

    Render3DObjects(m_direct3D->GetDeviceContext(), m_viewMatrix, m_projMatrix);
  
    RenderWorldSprites(m_direct3D->GetDeviceContext(), m_viewMatrix, m_projMatrix);


    if(m_compartmentRenderer&& m_colorShader)
    {
        // Убеждаемся, что растеризатор в режиме SOLID (заполнение)
        // Если m_wireframeRS — это wireframe, то нужно либо:
        // а) Создать отдельный m_solidRS, либо
        // б) Временно сбросить на nullptr (стандартный solid)

        ID3D11RasterizerState* oldRS = nullptr;
        m_direct3D->GetDeviceContext()->RSGetState(&oldRS);

        // Переключаем на SOLID для отсеков
        m_direct3D->GetDeviceContext()->RSSetState(nullptr); // nullptr = стандартный solid

        m_compartmentRenderer->RenderCompartments(
            m_sceneManager.GetECSRegistry(),
            m_direct3D->GetDeviceContext(),
            m_viewMatrix,
            m_projMatrix,
            m_colorShader
        );

        // Восстанавливаем предыдущий состояние (может быть wireframe)
        m_direct3D->GetDeviceContext()->RSSetState(oldRS);
        if (oldRS) oldRS->Release();
    }

    // ════════════════════════════════════════════════════════════════
    // 2. РЕНДЕР ФИЗИКИ (wireframe, только если включено в настройках)
    // ════════════════════════════════════════════════════════════════
    if (m_physicsDebugSettings.showColliders || m_physicsDebugSettings.showAABB || m_physicsDebugSettings.showContacts)
    {
        ID3D11RasterizerState* oldRS = nullptr;
        m_direct3D->GetDeviceContext()->RSGetState(&oldRS);

        // Включаем wireframe ТОЛЬКО для физики
        m_direct3D->GetDeviceContext()->RSSetState(m_wireframeRS);

        if (m_physicsDebugRenderer && m_colorShader)
        {
            m_physicsDebugRenderer->Render(
                m_sceneManager.GetECSRegistry(),
                PhysicsWorld::Get().GetWorld(),
                m_direct3D->GetDeviceContext(),
                m_viewMatrix,
                m_projMatrix,
                m_colorShader,
                m_physicsDebugSettings
            );
        }

        // Восстанавливаем
        m_direct3D->GetDeviceContext()->RSSetState(oldRS);
        if (oldRS) oldRS->Release();
    }

    // УДАЛЁН: прежний вызов UpdateFromWorld здесь — он мешал отображению маркеров (обновление происходило ПОСЛЕ рендера)

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

    // ════════════════════════════════════════════════════════════════════════════
// 🔥 ImGuizmo для редактирования отсеков
// ════════════════════════════════════════════════════════════════════════════
    if (m_editorUI.IsEditingCompartment() && m_compartmentRenderer)
    {
        auto& registry = m_sceneManager.GetECSRegistry();
        auto* comp = registry.GetComponent<CompartmentComponent>(m_editorUI.GetEditingCompartmentEntity());

        if (comp && m_camera)
        {
            // Получаем матрицу подлодки (или мира, если подлодки нет)
            XMMATRIX worldMatrix = XMMatrixIdentity();

            auto& subPool = registry.GetComponents<SubmarineComponent>();
            auto& transformPool = registry.GetComponents<TransformComponent>();

            auto* subLink = subPool.Get(m_editorUI.GetEditingCompartmentEntity());
            if (subLink && subLink->submarineRoot != INVALID_ENTITY)
            {
                auto* rootTransform = transformPool.Get(subLink->submarineRoot);
                if (rootTransform)
                    worldMatrix = rootTransform->GetWorldMatrix();
            }
            else
            {
                auto* selfTransform = transformPool.Get(m_editorUI.GetEditingCompartmentEntity());
                if (selfTransform)
                    worldMatrix = selfTransform->GetWorldMatrix();
            }

            // Включаем ImGuizmo
            ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
            ImGuizmo::SetRect(0, 0, ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);
         

            // Матрицы для гизмо
            XMMATRIX view, projection;
            m_camera->GetViewMatrix(view);
            m_direct3D->GetProjectionMatrix(projection);  // Или m_camera->GetProjectionMatrix(projection);

            // Текущая трансформация отсека (центр + размер как масштаб)
            XMMATRIX compartmentTransform =
                XMMatrixScaling(m_editorUI.GetCompartmentSize().x,
                    m_editorUI.GetCompartmentSize().y,
                    m_editorUI.GetCompartmentSize().z) *
                XMMatrixTranslation(m_editorUI.GetCompartmentCenter().x,
                    m_editorUI.GetCompartmentCenter().y,
                    m_editorUI.GetCompartmentCenter().z);

            // Конвертируем в массивы для ImGuizmo
            float matrix[16];
            XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(matrix), compartmentTransform);

            // Определяем операцию
            ImGuizmo::OPERATION op = m_editorUI.GetCompartmentEditMode() == EditorUI::CompartmentEditMode::Move
                ? ImGuizmo::TRANSLATE
                : ImGuizmo::SCALE;

            // Рисуем гизмо
            if (ImGuizmo::Manipulate(
                reinterpret_cast<const float*>(&view),
                reinterpret_cast<const float*>(&projection),
                op,
                ImGuizmo::LOCAL,  // Локальное пространство (относительно подлодки)
                matrix))
            {
                // Если гизмо изменился — обновляем кэш
                XMFLOAT4X4 floatMat;
                memcpy(&floatMat, matrix, sizeof(XMFLOAT4X4));  // Копируем данные из массива float[16]
                XMMATRIX newTransform = XMLoadFloat4x4(&floatMat);  // Конвертируем в XMMATRIX

                // Извлекаем позицию и масштаб
                XMFLOAT3 newCenter, newScale;
                XMStoreFloat3(&newCenter, newTransform.r[3]);  // Translation
                XMStoreFloat3(&newScale, newTransform.r[0]);   // Scale X (предполагаем равномерный масштаб)

                // Обновляем кэш редактора
                m_editorUI.SetCompartmentCenter(newCenter);
                m_editorUI.SetCompartmentSize(newScale);

                // Применяем к компоненту
                m_editorUI.ApplyCompartmentCacheToComponent(comp);
            }
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

    m_sceneManager.RenderAllECS(
        context, view, projection,
        m_defaultMaterial,
        lightColors, lightPositions,
        m_sceneManager.GetFogSettings(),
        selectedEntity,
        camPos);

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
        excludeEntity);
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

    registry.AddComponent<RigidBodyComponent>(subRoot, subRB);
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
        {
            char buf[256];
            sprintf_s(buf, "  → Shadow: entity=%u, shader=%s, pos=(%.1f, %.1f, %.1f)\n",
                entity,
                (st == ShaderType::Light) ? "Light" : "PBR",
                transform->position.x, transform->position.y, transform->position.z);
            OutputDebugStringA(buf);
        }

        renderedCount++;
    }

    // ВАЖНЫЙ ЛОГ
    {
        char buf[256];
        sprintf_s(buf, "✅ Render3DObjectsToShadowMap: %d objects rendered\n\n", renderedCount);
        OutputDebugStringA(buf);
    }

    // Очищаем состояние
    {
        ID3D11Buffer* nullBuffers[6] = {};
        context->VSSetConstantBuffers(0, 6, nullBuffers);
        context->PSSetConstantBuffers(0, 6, nullBuffers);

        ID3D11ShaderResourceView* nullSRVs[8] = {};
        context->PSSetShaderResources(0, 8, nullSRVs);
    }
}