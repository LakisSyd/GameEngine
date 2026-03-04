//=============================================================================
// ApplicationClass.cpp - ПРАВИЛЬНАЯ ВЕРСИЯ ДЛЯ DRAG AND DROP
// ИСПРАВЛЕНА ЛОГИКА ПЕРЕМЕЩЕНИЯ ОБЪЕКТОВ
//=============================================================================

#include "applicationclass.h"

ApplicationClass::ApplicationClass()
    : m_direct3D(nullptr)
    , m_camera(nullptr)
    , m_input(nullptr)
    , m_textureShader(nullptr)
    , m_lightShader(nullptr)
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
    , m_floorModel(nullptr)
    , m_wallModel(nullptr)
    , m_clickStartX(0), m_clickStartY(0)
    , m_potentialDrag(false)
    , m_defaultMaterial(nullptr)
{
}

bool ApplicationClass::Initialize(int screenWidth, int screenHeight, HWND hwnd)
{


    

    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;

    m_direct3D = new D3DClass;
    if (!m_direct3D) return false;

    if (!m_direct3D->Initialize(screenWidth, screenHeight, VSYNC_ENABLED, hwnd,
        FULL_SCREEN, SCREEN_DEPTH, SCREEN_NEAR))
    {
        MessageBox(hwnd, L"Failed to initialize Direct3D", L"Error", MB_OK);
        return false;
    }

    if (!m_editorUI.Initialize(hwnd, m_direct3D->GetDevice(), m_direct3D->GetDeviceContext()))
    {
        return false;
    }

  

    m_input = new InputClass;
    if (!m_input) return false;
    m_input->Initialize();

    m_camera = new CameraClass;
    if (!m_camera) return false;
    m_camera->SetPosition(0.0f, 2.0f, -10.0f);
    m_camera->SetRotation(0.0f, 0.0f, 0.0f);

    m_mainModel = new AssimpModelClass;
    if (!m_mainModel) return false;

    if (!m_mainModel->Initialize(m_direct3D->GetDevice(), m_direct3D->GetDeviceContext(),
        "assets/model/sphere.obj", "assets/texture/stone.tga"))
    {
        MessageBox(hwnd, L"Failed to load main model", L"Error", MB_OK);
        return false;
    }

    m_sceneManager.SetFallbackModel(m_mainModel);  // 🔥 Важно!

    m_lightSphereModel = new AssimpModelClass;
    if (!m_lightSphereModel) return false;

    if (!m_lightSphereModel->Initialize(m_direct3D->GetDevice(), m_direct3D->GetDeviceContext(),
        "assets/model/sphere.obj", "assets/texture/stone.tga"))
    {
        MessageBox(hwnd, L"Failed to load light sphere model", L"Error", MB_OK);
        return false;
    }

    m_textureShader = new TextureShaderClass;
    if (!m_textureShader || !m_textureShader->Initialize(m_direct3D->GetDevice(), hwnd))
    {
        MessageBox(hwnd, L"Failed to initialize texture shader", L"Error", MB_OK);
        return false;
    }

    m_lightShader = new LightShaderClass;
    if (!m_lightShader || !m_lightShader->Initialize(m_direct3D->GetDevice(), hwnd))
    {
        MessageBox(hwnd, L"Failed to initialize light shader", L"Error", MB_OK);
        return false;
    }

    m_defaultMaterial = new MaterialClass();
    m_defaultMaterial->Initialize(m_direct3D->GetDevice(), L"DefaultLighted");
    m_defaultMaterial->SetShader(m_lightShader);

    auto* blueMat = new MaterialClass();
    blueMat->Initialize(m_direct3D->GetDevice(), L"Blue");
    blueMat->SetShader(m_lightShader);
    blueMat->SetDiffuseColor(XMFLOAT3(0.2f, 0.4f, 1.0f));

    

    // 🔥 Загружаем текстуру!
    TextureClass* defaultTexture = new TextureClass();
    if (defaultTexture->Initialize(m_direct3D->GetDevice(),
        m_direct3D->GetDeviceContext(),
        "assets/texture/stone.tga"))
    {
        m_defaultMaterial->SetMainTexture(defaultTexture);
    }

   

    m_sceneManager.SetDefaultMaterial(m_defaultMaterial);

    int spriteIndex = m_sceneManager.SpawnSprite(
        m_direct3D->GetDevice(),
        m_direct3D->GetDeviceContext(),
        "assets/texture/arrow.tga",
        5.0f, 1.5f, -3.0f,
        0.3f, 0.3f
    );

    

    m_sceneManager.SpawnObject(m_mainModel, 0.0f, 0.0f, 0.0f);
    m_sceneManager.SpawnObject(m_mainModel, 3.0f, 0.0f, 0.0f);
   

    m_sceneManager.SetLight(0, XMFLOAT3(-1.0f, 2.0f, -1.0f),
        XMFLOAT4(1.0f, 1.0f, 0.9f, 1.0f), true);

    m_sceneManager.SetLight(1, XMFLOAT3(0, 0, 0), XMFLOAT4(1.0f, 0.8f, 0.4f, 1.0f), true);
    m_sceneManager.AttachLightToObject(1, 0, XMFLOAT3(0, 1.0f, 0));

    m_sceneManager.SetupLightSpheres(m_lightSpheres, m_lightSphereModel);


    

    return true;
}

void ApplicationClass::Shutdown()
{
    if (m_lightSphereModel)
    {
        m_lightSphereModel->Shutdown();
        delete m_lightSphereModel;
        m_lightSphereModel = nullptr;
    }

    if (m_lightShader)
    {
        m_lightShader->Shutdown();
        delete m_lightShader;
        m_lightShader = nullptr;
    }
    if (m_textureShader)
    {
        m_textureShader->Shutdown();
        delete m_textureShader;
        m_textureShader = nullptr;
    }

    if (m_mainModel)
    {
        m_mainModel->Shutdown();
        delete m_mainModel;
        m_mainModel = nullptr;
    }

    if (m_camera)
    {
        delete m_camera;
        m_camera = nullptr;
    }
    if (m_input)
    {
        delete m_input;
        m_input = nullptr;
    }

    m_editorUI.Shutdown();

    if (m_direct3D)
    {
        m_direct3D->Shutdown();
        delete m_direct3D;
        m_direct3D = nullptr;
    }
}

//=============================================================================
// Frame() - ГЛАВНЫЙ ЦИКЛ
//=============================================================================
bool ApplicationClass::Frame()
{
    // 1️⃣ Обновляем состояние ввода
    m_inputManager.Update(m_input);
    ImGuiIO& io = ImGui::GetIO();
    bool uiWantsMouse = io.WantCaptureMouse || ImGuizmo::IsOver() || ImGuizmo::IsUsing();

   
   HandleCameraMovement();  // ← Переименованная функция (см. ниже)
    
    m_sceneManager.UpdateLights();

    // 2️⃣ Начинаем рендер
    m_direct3D->BeginScene(0.0f, 0.0f, 0.0f, 1.0f);

    // 3️⃣ Получаем матрицы
    XMMATRIX viewMatrix, projMatrix;
    m_camera->Render();
    m_camera->GetViewMatrix(viewMatrix);
    m_direct3D->GetProjectionMatrix(projMatrix);

    m_viewMatrix = viewMatrix;
    m_projMatrix = projMatrix;

    ID3D11DeviceContext* context = m_direct3D->GetDeviceContext();

    // 4️⃣ Рендерим 3D
    Render3DObjects(context, viewMatrix, projMatrix);
    RenderLightSources(context, viewMatrix, projMatrix);
    RenderWorldSprites(context, viewMatrix, projMatrix);

    // 5️⃣ Начинаем UI
    m_editorUI.NewFrame();

    // 6️⃣ Рендерим ImGuizmo для выбранного объекта
    RenderImGuizmo(viewMatrix, projMatrix);

    // 7️⃣ 🔥 ОБРАБОТКА DRAG AND DROP 
    // ⚠️ Проверяем не захватила ли ImGui мышь
   
    bool imguiWantsMouse = io.WantCaptureMouse || ImGuizmo::IsOver() || ImGuizmo::IsUsing();

    if (!imguiWantsMouse)
    {
        HandleDragDrop();
    }

    // 8️⃣ Обновляем UI
    m_editorUI.Update(m_sceneManager);
    m_editorUI.Render();

    // 9️⃣ Завершаем кадр
    m_direct3D->EndScene();

    return true;
}

void ApplicationClass::RenderImGuizmo(const XMMATRIX& viewMatrix, const XMMATRIX& projMatrix)
{
    // === 1. Проверка объекта ===
    int selectedIndex = m_editorUI.GetSelectedIndex();
    if (selectedIndex < 0) return;

    GameObject* obj = m_sceneManager.GetObject(selectedIndex);
    if (!obj) return;

    // === 2. Матрицы — БЕЗ транспонирования (как у тебя работало) ===
    XMMATRIX modelMatrix = obj->GetWorldMatrix();

    XMFLOAT4X4 viewFloat, projFloat, modelFloat;
    XMStoreFloat4x4(&viewFloat, viewMatrix);      // 🔥 NO transpose
    XMStoreFloat4x4(&projFloat, projMatrix);      // 🔥 NO transpose
    XMStoreFloat4x4(&modelFloat, modelMatrix);    // 🔥 NO transpose

    // === 3. 🔥 ИСПРАВЛЕНИЕ №1: Реальный размер окна ===
    ImGuiIO& io = ImGui::GetIO();
    float width = io.DisplaySize.x;
    float height = io.DisplaySize.y;

    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
    ImGuizmo::SetRect(0.0f, 0.0f, width, height);  // 🔥 Было: 800, 600
    ImGuizmo::SetGizmoSizeClipSpace(0.3f);  // 🔥 Вызови ПЕД Manipulate()!

    // === 4. 🔥 ИСПРАВЛЕНИЕ №2: Используем возврат Manipulate() ===
    bool manipulated = ImGuizmo::Manipulate(
        &viewFloat.m[0][0],   // Прямо как у тебя
        &projFloat.m[0][0],
        ImGuizmo::OPERATION::TRANSLATE,
        ImGuizmo::WORLD,
        &modelFloat.m[0][0]
    );

    // === 5. 🔥 ИСПРАВЛЕНИЕ №3: Проверяем manipulated, а не IsUsing() ===
    if (manipulated)  // 🔥 Было: if (ImGuizmo::IsUsing())
    {
        // Конвертируем обратно — тоже БЕЗ транспонирования
        XMMATRIX updatedMatrix = XMLoadFloat4x4(&modelFloat);  // 🔥 NO transpose

        XMFLOAT3 newPos;
        XMStoreFloat3(&newPos, updatedMatrix.r[3]);

        obj->SetPosition(newPos.x, newPos.y, newPos.z);
        OutputDebugStringA(("✏️ Gizmo moved: " +
            std::to_string(newPos.x) + ", " +
            std::to_string(newPos.y) + "\n").c_str());
    }
}

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
// 🔥 ИСПРАВЛЕННАЯ ЛОГИКА DRAG AND DROP
//=============================================================================

void ApplicationClass::UpdateInputState()
{
    m_isLeftDown = m_input->IsLeftButtonDown();
    m_isRightDown = m_input->IsRightButtonDown();
    m_isMouseCaptured = m_input->IsMouseCaptured();

    if (!m_isMouseCaptured)
    {
        m_input->GetMousePosition(m_mouseX, m_mouseY);
    }

    static bool wasLeftDown = false;

    // 👇 ЛОГИКА: только при НОВОМ нажатии
    if (m_isLeftDown && !wasLeftDown)
    {
        m_clickStartX = m_mouseX;
        m_clickStartY = m_mouseY;
        m_potentialDrag = true;
        OutputDebugStringA("🖱️ Left mouse button DOWN\n");
    }

    wasLeftDown = m_isLeftDown;
}

// Убираем всю логику кнопок мыши — это теперь в InputManager!
void ApplicationClass::HandleCameraMovement()
{
    // === 1. Захват/освобождение мыши (ПРЯМАЯ проверка!) ===
    bool rightDown = m_input->IsRightButtonDown();  // ← Прямой запрос, без InputManager

    if (rightDown && !m_input->IsMouseCaptured()) {
        m_input->SetMouseCapture(true);
    }
    else if (!rightDown && m_input->IsMouseCaptured()) {
        m_input->SetMouseCapture(false);  // ← Здесь должен появиться курсор!
    }

    // === 2. Вращение камеры (ТОЛЬКО при захваченной мыши) ===
    if (m_input->IsMouseCaptured()) {
        int deltaX, deltaY;
        m_input->GetMouseDelta(deltaX, deltaY);
        m_input->ResetMouseDelta();

        float rotX = m_camera->GetRotation().x;
        float rotY = m_camera->GetRotation().y;

        const float sensitivity = Settings::CAMERA_SENSITIVITY;
        rotY += deltaX * sensitivity;
        rotX += deltaY * sensitivity;
        rotX = clamp(rotX, -89.0f, 89.0f);

        m_camera->SetRotation(rotX, rotY, m_camera->GetRotation().z);
    }

    // === 3. 🔥 ПЕРЕМЕЩЕНИЕ (РАБОТАЕТ ВСЕГДА, независимо от захвата!) ===
    {
        float posX = m_camera->GetPosition().x;
        float posY = m_camera->GetPosition().y;
        float posZ = m_camera->GetPosition().z;

        float speed = Settings::INPUT_MOVE_SPEED;
        if (m_input->IsKeyDown(VK_SHIFT))    speed *= 2.0f;
        if (m_input->IsKeyDown(VK_CONTROL))  speed *= 0.5f;

        float yawRad = XMConvertToRadians(m_camera->GetRotation().y);
        float sinYaw = sinf(yawRad);
        float cosYaw = cosf(yawRad);

        if (m_input->IsKeyDown('W')) { posX += sinYaw * speed; posZ += cosYaw * speed; }
        if (m_input->IsKeyDown('S')) { posX -= sinYaw * speed; posZ -= cosYaw * speed; }
        if (m_input->IsKeyDown('A')) { posX -= cosYaw * speed; posZ += sinYaw * speed; }
        if (m_input->IsKeyDown('D')) { posX += cosYaw * speed; posZ -= sinYaw * speed; }
        if (m_input->IsKeyDown('R')) { posY += speed; }
        if (m_input->IsKeyDown('F')) { posY -= speed; }

        m_camera->SetPosition(posX, posY, posZ);  // ← Применяем всегда
    }
}
//=============================================================================
// 🔥 ПРАВИЛЬНАЯ ЛОГИКА DRAG AND DROP
//=============================================================================
void ApplicationClass::HandleDragDrop()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse || ImGuizmo::IsOver()) return;  // Игнорируем, если UI активен

    // === 1. Начало drag (через InputManager события) ===
    if (m_inputManager.IsDragging() && m_inputManager.IsLeftDown())
    {
        // Создаём луч по текущей позиции мыши
        CameraClass::Ray ray = m_camera->CreatePickingRay(
            m_inputManager.GetMouseX(),
            m_inputManager.GetMouseY(),
            m_screenWidth, m_screenHeight,
            m_viewMatrix, m_projMatrix  // ← матрицы из Frame()
        );

        UpdateDraggedPosition(ray);  // Ваша существующая функция
        return;
    }

    // === 2. Быстрый клик — выбор объекта через PickingSystem ===
    if (!m_inputManager.IsDragging() && !m_inputManager.IsCameraMode())
    {
        auto result = m_pickingSystem.PickAt(
            m_inputManager.GetMouseX(),
            m_inputManager.GetMouseY(),
            *m_camera,
            m_viewMatrix,        // ← Вид отдельно
            m_projMatrix,        // ← Проекция отдельно
            m_sceneManager,
            m_lightSpheres,
            m_screenWidth,
            m_screenHeight
        );

        if (result.IsValid())
        {
            // 🔥 Конвертируем тип из PickingSystem в EditorUI::SelectedType
            EditorUI::SelectedType uiType = EditorUI::SelectedType::None;
            switch (result.type) {
            case PickingSystem::PickedType::Object: uiType = EditorUI::SelectedType::Object; break;
            case PickingSystem::PickedType::Light:  uiType = EditorUI::SelectedType::Light; break;
            case PickingSystem::PickedType::Sprite: uiType = EditorUI::SelectedType::Sprite; break;
            }

            m_editorUI.SetSelection(uiType, result.index);

            // 🔥 Если выбран свет — начинаем drag сразу
            if (result.type == PickingSystem::PickedType::Light) {
                StartDraggingLight(result.index, {});  // Пустой Ray, позиция не важна
            }
        }
    }
}



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