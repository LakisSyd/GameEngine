//=============================================================================
// ApplicationClass.cpp - Упрощённая и документированная версия
// ✨ Рефакторинг: добавлены комментарии, небольшая структуризация, вызов
// UpdateInputState() в Frame() для синхронизации состояния мыши.
//=============================================================================

#include "applicationclass.h"   // Добавлен include для NormalMapShaderClass


ApplicationClass::ApplicationClass()
    :

    m_textureShader(nullptr)
    , m_lightShader(nullptr)
	, m_multiTextureShader(nullptr)
	, m_lightMapShader(nullptr)
    , m_mainModel(nullptr)
    , m_lightSphereModel(nullptr)
    , m_isLeftDown(false)
    , m_isRightDown(false)
    , m_isMouseCaptured(false)
    , m_mouseX(0), m_mouseY(0)
    , m_isDragging(false)
    , m_draggedObjectIndex(-1)
    , m_draggedLightIndex(-1)
    , m_draggedSpriteIndex(-1)
    , m_dragDistance(0.0f)
    , m_dragOffset{ 0,0,0 }
    , m_clickStartX(0), m_clickStartY(0)
    , m_potentialDrag(false)
    , m_defaultMaterial(nullptr)
    , m_normalMapShader(nullptr)
{
}

bool ApplicationClass::Initialize(int screenWidth, int screenHeight, HWND hwnd)
{
    // Сохраняем размеры окна
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;

    m_direct3D = std::make_unique<D3DClass>();

    if (!m_direct3D) return false;

    if (!m_direct3D->Initialize(screenWidth, screenHeight, VSYNC_ENABLED, hwnd,
        FULL_SCREEN, SCREEN_DEPTH, SCREEN_NEAR))
    {
        MessageBox(hwnd, L"Failed to initialize Direct3D", L"Error", MB_OK);
        return false;
    }

    // Инициализация UI (ImGui + Editor)
    if (!m_editorUI.Initialize(hwnd, m_direct3D->GetDevice(), m_direct3D->GetDeviceContext()))
    {
        return false;
    }

    // NOTE:
    // InputClass НЕ создаётся здесь — SystemClass владеет Input и передаёт указатель через SetInput().
    // Если Application должен уметь работать автономно, можно добавить защиту от nullptr в методах.
    // (m_input будет установлен SystemClass::Initialize после вызова Application::Initialize)

    // Камера
    m_camera = std::make_unique<CameraClass>();
    if (!m_camera) return false;
    m_camera->SetPosition(0.0f, 2.0f, -10.0f);
    m_camera->SetRotation(0.0f, 0.0f, 0.0f);

    // Загрузка моделей
    m_mainModel = new AssimpModelClass;
    if (!m_mainModel) return false;
    if (!m_mainModel->Initialize(m_direct3D->GetDevice(), m_direct3D->GetDeviceContext(),
        "assets/model/sphere.obj", "assets/texture/stone.tga"))
    {
        MessageBox(hwnd, L"Failed to load main model", L"Error", MB_OK);
        return false;
    }

    m_sceneManager.SetFallbackModel(m_mainModel);  // запасная модель для менеджера сцен

    m_lightSphereModel = new AssimpModelClass;
    if (!m_lightSphereModel) return false;
    if (!m_lightSphereModel->Initialize(m_direct3D->GetDevice(), m_direct3D->GetDeviceContext(),
        "assets/model/sphere.obj", "assets/texture/stone.tga"))
    {
        MessageBox(hwnd, L"Failed to load light sphere model", L"Error", MB_OK);
        return false;
    }

    // ════════════════════════════════════
   // Загрузка шейдеров через ShaderManager
   // ════════════════════════════════════
    auto& sm = ShaderManager::Get();

    m_textureShader = sm.Load<TextureShaderClass>("texture", m_direct3D->GetDevice(), hwnd);
    m_lightShader = sm.Load<LightShaderClass>("light", m_direct3D->GetDevice(), hwnd);
    m_multiTextureShader = sm.Load<MultiTextureShaderClass>("multitex", m_direct3D->GetDevice(), hwnd);
    m_lightMapShader = sm.Load<LightMapShaderClass>("lightmap", m_direct3D->GetDevice(), hwnd);
    m_normalMapShader = sm.Load<NormalMapShaderClass>("normalmap", m_direct3D->GetDevice(), hwnd);

    if (!m_textureShader || !m_lightShader ||
        !m_multiTextureShader || !m_lightMapShader || !m_normalMapShader)
    {
        return false; // ошибка уже показана через MessageBox внутри Load()
    }

    // Материалы
    m_defaultMaterial = new MaterialClass();
    m_defaultMaterial->Initialize(m_direct3D->GetDevice(), L"DefaultLighted");
    m_defaultMaterial->SetShader(m_lightShader);

    auto* blueMat = new MaterialClass();
    blueMat->Initialize(m_direct3D->GetDevice(), L"Blue");
    blueMat->SetShader(m_lightShader);
    blueMat->SetDiffuseColor(XMFLOAT3(0.2f, 0.4f, 1.0f));

    // Текстура по умолчанию (назначаем на материал)
    TextureClass* defaultTexture = new TextureClass();
    if (defaultTexture->Initialize(m_direct3D->GetDevice(),
        m_direct3D->GetDeviceContext(),
        "assets/texture/stone.tga"))
    {
        m_defaultMaterial->SetMainTexture(defaultTexture);
    }

    m_sceneManager.SetDefaultMaterial(m_defaultMaterial);

    // Примитивный спрайт в мире
    int spriteIndex = m_sceneManager.SpawnSprite(
        m_direct3D->GetDevice(),
        m_direct3D->GetDeviceContext(),
        "assets/texture/arrow.tga",
        5.0f, 1.5f, -3.0f,
        0.3f, 0.3f
    );

    // Пара объектов для примера
    m_sceneManager.SpawnObject(m_mainModel, 0.0f, 0.0f, 0.0f);
    m_sceneManager.SpawnObject(m_mainModel, 3.0f, 0.0f, 0.0f);

    // Настройка света
    m_sceneManager.SetLight(0, XMFLOAT3(-1.0f, 2.0f, -1.0f),
        XMFLOAT4(1.0f, 1.0f, 0.9f, 1.0f), true);

    m_sceneManager.SetLight(1, XMFLOAT3(0, 0, 0), XMFLOAT4(1.0f, 0.8f, 0.4f, 1.0f), true);
    m_sceneManager.AttachLightToObject(1, 0, XMFLOAT3(0, 1.0f, 0));

    m_sceneManager.SetupLightSpheres(m_lightSpheres, m_lightSphereModel);
    // Пара объектов для примера — сохраняем индексы и назначаем материал
    int idx0 = m_sceneManager.SpawnObject(m_mainModel, 10.0f, 0.0f, 0.0f);
    int idx1 = m_sceneManager.SpawnObject(m_mainModel, 5.0f, 0.0f, 0.0f);

    auto* mat = new MaterialClass();
    mat->Initialize(m_direct3D->GetDevice(), L"StoneWithDirt");
    // назначаем мульти-шейдер (убедитесь, что m_multiTextureShader инициализирован)
    mat->SetShader(m_normalMapShader);

    TextureClass* stone = new TextureClass();
    bool okStone = stone->Initialize(m_direct3D->GetDevice(), m_direct3D->GetDeviceContext(), "assets/texture/stone.tga");
    if (okStone) mat->AddTexture(stone);
    else OutputDebugStringA("ERROR: stone texture failed to initialize\n");

    TextureClass* dirt = new TextureClass();
    bool okDirt = dirt->Initialize(m_direct3D->GetDevice(), m_direct3D->GetDeviceContext(), "assets/texture/uv.tga");
    if (okDirt) mat->AddTexture(dirt);
    else OutputDebugStringA("ERROR: dirt texture failed to initialize\n");

    // Печатаем указатели SRV — быстрое подтверждение, что GetTexture() не nullptr
    {
        char buf[256];
        sprintf_s(buf, "DEBUG: stone SRV=%p, dirt SRV=%p, okStone=%d, okDirt=%d\n",
            (void*)(stone->GetTexture()), (void*)(dirt->GetTexture()), okStone ? 1 : 0, okDirt ? 1 : 0);
        OutputDebugStringA(buf);
    }

    // Назначаем материал объекту (убедитесь, что берёте правильный объект)
    GameObject* obj0 = m_sceneManager.GetObject(idx0); // если у вас другое имя — используйте m_sceneManager
    GameObject* obj1 = m_sceneManager.GetObject(idx1);
    if (obj0) {
        obj0->SetMaterial(mat);
        OutputDebugStringA("DEBUG: Application: assigned MultiTexture material to obj0\n");
    }
    if (obj1) {
        obj1->SetMaterial(mat);
        OutputDebugStringA("DEBUG: Application: assigned MultiTexture material to obj1\n");
    }

   // Render - to - texture
        m_renderTexture = new RenderTextureClass;
    if (!m_renderTexture->Initialize(m_direct3D->GetDevice(),
        512, 512, SCREEN_DEPTH, SCREEN_NEAR, 1))
    {
        MessageBox(hwnd, L"Failed to initialize RenderTexture", L"Error", MB_OK);
        return false;
    }

    // Квадрат для отображения текстуры (позиция в пикселях)
    m_renderQuad = new BitmapClass;
	if (!m_renderQuad->Initialize(m_direct3D->GetDevice(), m_direct3D->GetDeviceContext(), screenHeight, screenWidth,
         256, 256))
    {
        MessageBox(hwnd, L"Failed to initialize RenderQuad", L"Error", MB_OK);
        return false;
    }
    m_renderQuad->SetRenderLocation(10, 10); // верхний левый угол

   
    return true;
}

void ApplicationClass::Shutdown()
{
    // Очистка ресурсов в обратном порядке создания
    if (m_lightSphereModel)
    {
        m_lightSphereModel->Shutdown();
        delete m_lightSphereModel;
        m_lightSphereModel = nullptr;
    }

    ShaderManager::Get().Shutdown();

    if (m_mainModel)
    {
        m_mainModel->Shutdown();
        delete m_mainModel;
        m_mainModel = nullptr;
    }


    if (m_direct3D) {
        m_direct3D->Shutdown();
    }

    m_editorUI.Shutdown();


}


//=============================================================================
// Frame() - основной цикл кадра
// - обновление ввода
// - обработка камеры / перемещения
// - рендер 3D + UI
// - обработка drag & drop
//=============================================================================
bool ApplicationClass::Frame()
{
    // 1) Обновляем InputManager
    m_inputManager.Update(m_input);
    ImGuiIO& io = ImGui::GetIO();

    // 2) Обработка камеры
    HandleCameraMovement();
    m_sceneManager.UpdateLights();

    // 3) Обновляем матрицы камеры ОДИН РАЗ до всех проходов
    m_camera->Render();
    m_camera->GetViewMatrix(m_viewMatrix);
    m_direct3D->GetProjectionMatrix(m_projMatrix);

    ID3D11DeviceContext* context = m_direct3D->GetDeviceContext();

    // ══════════════════════════════════════════
    // 🔹 ПРОХОД 1 — рендер сцены В ТЕКСТУРУ
    // ══════════════════════════════════════════
    m_renderTexture->SetRenderTarget(context);
    m_renderTexture->ClearRenderTarget(context, 0.1f, 0.1f, 0.3f, 1.0f);

    XMMATRIX rtProj;
    m_renderTexture->GetProjectionMatrix(rtProj);

    Render3DObjects(context, m_viewMatrix, rtProj);

    // ══════════════════════════════════════════
    // 🔹 ПРОХОД 2 — рендер на ЭКРАН
    // ══════════════════════════════════════════
    m_direct3D->SetBackBufferRenderTarget();
    m_direct3D->ResetViewport();
    m_direct3D->BeginScene(0.0f, 0.0f, 0.0f, 1.0f);

    // Основная 3D сцена
    Render3DObjects(context, m_viewMatrix, m_projMatrix);
    RenderLightSources(context, m_viewMatrix, m_projMatrix);
    RenderWorldSprites(context, m_viewMatrix, m_projMatrix);

    

    XMMATRIX world, ortho;
    m_direct3D->GetWorldMatrix(world);
    m_direct3D->GetOrthoMatrix(ortho);

   

    // ══════════════════════════════════════════
    // 🔹 UI — NewFrame ОБЯЗАТЕЛЬНО ПЕРВЫЙ
    // ══════════════════════════════════════════
    m_editorUI.NewFrame(); // ← сначала NewFrame!

    RenderImGuizmo(m_viewMatrix, m_projMatrix);

    // ✅ ImGui::Begin только ПОСЛЕ NewFrame
    ImGui::Begin("Scene Preview");
    ImGui::Image(
        (ImTextureID)m_renderTexture->GetShaderResourceView(),
        ImVec2(256, 256)
    );
    ImGui::End();

    bool imguiWantsMouse = io.WantCaptureMouse || ImGuizmo::IsOver() || ImGuizmo::IsUsing();
    if (!imguiWantsMouse)
    {
        HandleDragDrop();
    }

    m_editorUI.Update(m_sceneManager);
    m_editorUI.Render();

    m_direct3D->EndScene();

    return true;
}


//-----------------------------------------------------------------------------
// RenderImGuizmo()
// Описание: рендер и обработка ImGuizmo для редактируемого объекта.
// Примечания: матрицы передаются и используются БЕЗ транспонирования.
//-----------------------------------------------------------------------------
void ApplicationClass::RenderImGuizmo(const XMMATRIX& viewMatrix, const XMMATRIX& projMatrix)
{
    // 1) Если нет выбранного объекта — выходим
    int selectedIndex = m_editorUI.GetSelectedIndex();
    if (selectedIndex < 0) return;

    GameObject* obj = m_sceneManager.GetObject(selectedIndex);
    if (!obj) return;

    // 2) Получаем модельную матрицу объекта
    XMMATRIX modelMatrix = obj->GetWorldMatrix();

    // Храним матрицы в формате float4x4 (ImGuizmo ожидает float[4][4])
    XMFLOAT4X4 viewFloat, projFloat, modelFloat;
    XMStoreFloat4x4(&viewFloat, viewMatrix);    // NO transpose
    XMStoreFloat4x4(&projFloat, projMatrix);    // NO transpose
    XMStoreFloat4x4(&modelFloat, modelMatrix);  // NO transpose

    // 3) Настраиваем ImGuizmo под реальное окно (DisplaySize)
    ImGuiIO& io = ImGui::GetIO();
    float width = io.DisplaySize.x;
    float height = io.DisplaySize.y;

    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
    ImGuizmo::SetRect(0.0f, 0.0f, width, height);
    ImGuizmo::SetGizmoSizeClipSpace(0.3f);

    // 4) Вызываем Manipulate и обрабатываем результат
    bool manipulated = ImGuizmo::Manipulate(
        &viewFloat.m[0][0],
        &projFloat.m[0][0],
        ImGuizmo::OPERATION::TRANSLATE,
        ImGuizmo::WORLD,
        &modelFloat.m[0][0]
    );

    // 5) Если была модификация — применяем новую позицию к объекту
    if (manipulated)
    {
        XMMATRIX updatedMatrix = XMLoadFloat4x4(&modelFloat); // NO transpose
        XMFLOAT3 newPos;
        XMStoreFloat3(&newPos, updatedMatrix.r[3]); // четвертый столбец — позиция

        obj->SetPosition(newPos.x, newPos.y, newPos.z);
        OutputDebugStringA(("✏️ Gizmo moved: " +
            std::to_string(newPos.x) + ", " +
            std::to_string(newPos.y) + "\n").c_str());
    }
}

//-----------------------------------------------------------------------------
// Рендеринг сценических объектов, источников света и спрайтов
//-----------------------------------------------------------------------------
void ApplicationClass::Render3DObjects(
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection)
{
    m_direct3D->TurnZBufferOn();
    m_direct3D->TurnAlphaBlendingOff();

    XMFLOAT4 lightColors[NUM_LIGHTS];
    XMFLOAT4 lightPositions[NUM_LIGHTS];
    SetupLights(lightColors, lightPositions);

    int selectedIndex = m_editorUI.GetSelectedIndex();
    m_sceneManager.RenderAll(context, m_direct3D->GetDevice(), view, projection,
        m_defaultMaterial, lightColors, lightPositions, selectedIndex);
}

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
        // Пропускаем неактивные источники света
        if (lightPositions[i].w <= 0.0f ||
            (lightColors[i].x == 0 && lightColors[i].y == 0 && lightColors[i].z == 0))
        {
            continue;
        }

        m_lightSpheres[i].SetPosition(
            lightPositions[i].x,
            lightPositions[i].y,
            lightPositions[i].z
        );

        m_lightSpheres[i].RenderSimple(context, view, projection, m_textureShader);
    }
}

void ApplicationClass::RenderWorldSprites(
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection)
{
    m_direct3D->TurnAlphaBlendingOn();
    m_sceneManager.RenderSprites(context, view, projection, m_textureShader);
    m_direct3D->TurnAlphaBlendingOff();
}

//-----------------------------------------------------------------------------
// Вспомогательные функции: свет, матрицы, пересечение луч-сфера
//-----------------------------------------------------------------------------
void ApplicationClass::SetupLights(XMFLOAT4 colors[], XMFLOAT4 positions[])
{
    for (int i = 0; i < NUM_LIGHTS; ++i)
    {
        colors[i] = m_sceneManager.GetLightColor(i);
        positions[i] = m_sceneManager.GetLightPosition(i);
    }
}

XMMATRIX ApplicationClass::BuildSpriteMatrix(
    float x, float y, float z,
    float width, float height) const
{
    XMMATRIX scale = XMMatrixScaling(width, height, 1.0f);
    XMMATRIX translate = XMMatrixTranslation(x, y, z);
    return scale * translate;
}

bool RayIntersectsSphere(
    const CameraClass::Ray& ray,
    const XMFLOAT3& sphereCenter,
    float sphereRadius)
{
    XMVECTOR L = XMVectorSubtract(
        XMLoadFloat3(&sphereCenter),
        XMLoadFloat3(&ray.origin)
    );

    float t = XMVectorGetX(XMVector3Dot(L, XMLoadFloat3(&ray.direction)));
    if (t < 0.0f) return false;

    XMVECTOR closestPoint = XMVectorAdd(
        XMLoadFloat3(&ray.origin),
        XMVectorScale(XMLoadFloat3(&ray.direction), t)
    );

    float distance = XMVectorGetX(
        XMVector3Length(
            XMVectorSubtract(closestPoint, XMLoadFloat3(&sphereCenter))
        )
    );

    return distance <= sphereRadius;
}

//=============================================================================
// Обработка ввода и Drag&Drop
//=============================================================================
void ApplicationClass::UpdateInputState()
{
    // Синхронизируем локальные флаги из низкоуровневого ввода
    m_isLeftDown = m_input->IsLeftButtonDown();
    m_isRightDown = m_input->IsRightButtonDown();
    m_isMouseCaptured = m_input->IsMouseCaptured();
   /* m_isLeftDown = m_inputManager.IsLeftDown();*/

    if (!m_isMouseCaptured)
    {
        m_input->GetMousePosition(m_mouseX, m_mouseY);
    }

    static bool wasLeftDown = false;

    // Только при новом нажатии фиксируем координаты начала клика
    if (m_isLeftDown && !wasLeftDown)
    {
        m_clickStartX = m_mouseX;
        m_clickStartY = m_mouseY;
        m_potentialDrag = true;
        OutputDebugStringA("🖱️ Left mouse button DOWN\n");
    }

    wasLeftDown = m_isLeftDown;
}

void ApplicationClass::HandleCameraMovement()
{
    // ===== 1) ЗАХВАТ / ОСВОБОДЕНИЕ МЫШИ — ЧЕРЕЗ InputManager =====
    bool rightDown = m_inputManager.IsCameraMode();  // 🔥 Из InputManager, не m_input!

    if (rightDown && !m_inputManager.IsMouseCaptured(m_input)) {
        m_inputManager.SetMouseCapture(m_input, true);
    }
    else if (!rightDown && m_inputManager.IsMouseCaptured(m_input)) {
        m_inputManager.SetMouseCapture(m_input, false);
    }

    // ===== 2) ВРАЩЕНИЕ КАМЕРЫ — ТОЛЬКО ЕСЛИ ЗАХВАЧЕНА МЫШЬ =====
    if (m_inputManager.IsMouseCaptured(m_input)) {
        int deltaX, deltaY;
        m_input->GetMouseDelta(deltaX, deltaY);  // Сырой InputClass для дельты
        m_inputManager.ResetMouseDelta(m_input);

        float rotX = m_camera->GetRotation().x;
        float rotY = m_camera->GetRotation().y;

        const float sensitivity = Settings::CAMERA_SENSITIVITY;
        rotY += deltaX * sensitivity;
        rotX += deltaY * sensitivity;
        rotX = clamp(rotX, -89.0f, 89.0f);

        m_camera->SetRotation(rotX, rotY, m_camera->GetRotation().z);
    }

    // ===== 3) ПЕРЕМЕЩЕНИЕ КАМЕРЫ — ВСЕГДА =====
    {
        float posX = m_camera->GetPosition().x;
        float posY = m_camera->GetPosition().y;
        float posZ = m_camera->GetPosition().z;

        float speed = Settings::INPUT_MOVE_SPEED;
        if (m_inputManager.IsKeyDown(m_input, VK_SHIFT))    speed *= 2.0f;
        if (m_inputManager.IsKeyDown(m_input, VK_CONTROL))  speed *= 0.5f;

        float yawRad = XMConvertToRadians(m_camera->GetRotation().y);
        float sinYaw = sinf(yawRad);
        float cosYaw = cosf(yawRad);

        if (m_inputManager.IsKeyDown(m_input, 'W')) { posX += sinYaw * speed; posZ += cosYaw * speed; }
        if (m_inputManager.IsKeyDown(m_input, 'L')) { m_sceneManager.LoadScene("f.json", m_direct3D->GetDevice(), m_direct3D->GetDeviceContext()); }
        if (m_inputManager.IsKeyDown(m_input, 'S')) { posX -= sinYaw * speed; posZ -= cosYaw * speed; }
        if (m_inputManager.IsKeyDown(m_input, 'A')) { posX -= cosYaw * speed; posZ += sinYaw * speed; }
        if (m_inputManager.IsKeyDown(m_input, 'D')) { posX += cosYaw * speed; posZ -= sinYaw * speed; }
        if (m_inputManager.IsKeyDown(m_input, 'R')) { posY += speed; }
        if (m_inputManager.IsKeyDown(m_input, 'F')) { posY -= speed; }

        m_camera->SetPosition(posX, posY, posZ);
    }
}

void ApplicationClass::HandleDragDrop()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse || ImGuizmo::IsOver()) return;

    // 1) Если уже dragging — обновляем позицию
    if (m_inputManager.IsDragging() && m_inputManager.IsLeftDown())
    {
        CameraClass::Ray ray = m_camera->CreatePickingRay(
            m_inputManager.GetMouseX(),
            m_inputManager.GetMouseY(),
            m_screenWidth, m_screenHeight,
            m_viewMatrix, m_projMatrix
        );

        UpdateDraggedPosition(ray);
        return;
    }

    // 2) Быстрый клик — выбор объекта
    if (!m_inputManager.IsDragging() && !m_inputManager.IsCameraMode())
    {
        auto result = m_pickingSystem.PickAt(
            m_inputManager.GetMouseX(),
            m_inputManager.GetMouseY(),
            *m_camera,
            m_viewMatrix,
            m_projMatrix,
            m_sceneManager,
            m_lightSpheres,
            m_screenWidth,
            m_screenHeight
        );

        if (result.IsValid())
        {
            EditorUI::SelectedType uiType = EditorUI::SelectedType::None;
            switch (result.type) {
            case PickingSystem::PickedType::Object: uiType = EditorUI::SelectedType::Object; break;
            case PickingSystem::PickedType::Light:  uiType = EditorUI::SelectedType::Light; break;
            case PickingSystem::PickedType::Sprite: uiType = EditorUI::SelectedType::Sprite; break;
            }

            m_editorUI.SetSelection(uiType, result.index);

            // 🔥 НОВОЕ: Начинаем drag для ВСЕХ типов, не только света
            CameraClass::Ray ray = m_camera->CreatePickingRay(
                m_inputManager.GetMouseX(),
                m_inputManager.GetMouseY(),
                m_screenWidth, m_screenHeight,
                m_viewMatrix, m_projMatrix
            );

            if (result.type == PickingSystem::PickedType::Light)
            {
                StartDraggingLight(result.index, ray);
            }
            else if (result.type == PickingSystem::PickedType::Object)
            {
                GameObject* obj = m_sceneManager.GetObject(result.index);
                if (obj)
                {
                    XMFLOAT3 objPos = obj->GetPosition();  // или GetWorldMatrix().r[3]
                    StartDraggingObject(result.index, ray, objPos);
                }
            }
            else if (result.type == PickingSystem::PickedType::Sprite)
            {
                WorldSpriteClass* sprite = m_sceneManager.GetSprite(result.index);
                if (sprite)
                {
                    XMFLOAT3 spritePos = sprite->GetPosition();
                    StartDraggingSprite(result.index, ray, spritePos);
                }
            }
        }
    }
}
//-----------------------------------------------------------------------------
// Начало перетаскивания (light / sprite / object) и обновление позиции
//-----------------------------------------------------------------------------
void ApplicationClass::StartDraggingLight(int index, const CameraClass::Ray& pickRay)
{
    m_isDragging = true;
    m_draggedLightIndex = index;
    m_draggedObjectIndex = -1;
    m_draggedSpriteIndex = -1;
    m_dragDistance = 2.0f;
    m_dragOffset = { 0,0,0 };
    m_sceneManager.DetachLight(index);
    OutputDebugStringA("💡 Dragging light\n");
}

void ApplicationClass::StartDraggingSprite(int index, const CameraClass::Ray& pickRay, const XMFLOAT3& spritePos)
{
    m_isDragging = true;
    m_draggedSpriteIndex = index;
    m_draggedLightIndex = -1;
    m_draggedObjectIndex = -1;

    XMVECTOR toSprite = XMLoadFloat3(&spritePos) - XMLoadFloat3(&pickRay.origin);
    m_dragDistance = XMVectorGetX(XMVector3Length(toSprite));
    if (m_dragDistance < 1.0f) m_dragDistance = 1.0f;

    m_dragOffset = { 0,0,0 };
    OutputDebugStringA("🖼️ Dragging sprite\n");
}

void ApplicationClass::StartDraggingObject(int index, const CameraClass::Ray& pickRay, const XMFLOAT3& objPos)
{
    m_isDragging = true;
    m_draggedObjectIndex = index;
    m_draggedLightIndex = -1;
    m_draggedSpriteIndex = -1;

    XMVECTOR toObj = XMLoadFloat3(&objPos) - XMLoadFloat3(&pickRay.origin);
    m_dragDistance = XMVectorGetX(XMVector3Length(toObj));
    if (m_dragDistance < 1.0f) m_dragDistance = 1.0f;

    m_dragOffset = { 0,0,0 };
    OutputDebugStringA("📦 Dragging object\n");
}

void ApplicationClass::UpdateDraggedPosition(const CameraClass::Ray& ray)
{
    // Простейшее позиционирование вдоль луча на фиксированном расстоянии
    XMFLOAT3 newPos;
    newPos.x = ray.origin.x + ray.direction.x * m_dragDistance;
    newPos.y = ray.origin.y + ray.direction.y * m_dragDistance;
    newPos.z = ray.origin.z + ray.direction.z * m_dragDistance;

    newPos.x += m_dragOffset.x;
    newPos.y += m_dragOffset.y;
    newPos.z += m_dragOffset.z;

    if (m_draggedLightIndex >= 0)
    {
        m_sceneManager.SetLight(m_draggedLightIndex, newPos,
            XMFLOAT4(1, 1, 0.9f, 1), true);
    }
    else if (m_draggedSpriteIndex >= 0)
    {
        WorldSpriteClass* sprite = m_sceneManager.GetSprite(m_draggedSpriteIndex);
        if (sprite) sprite->SetPosition(newPos.x, newPos.y, newPos.z);
    }
    else if (m_draggedObjectIndex >= 0)
    {
        GameObject* obj = m_sceneManager.GetObject(m_draggedObjectIndex);
        if (obj)
        {
            obj->SetPosition(newPos.x, newPos.y, newPos.z);
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