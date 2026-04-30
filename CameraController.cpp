#include "CameraController.h"
#include "EditorUI.h"
#include "GameSettings.h"
#include <algorithm>

CameraController::CameraController(CameraClass* camera,
    InputManager* inputManager,
    InputClass* input,
    EditorUI* editorUI)
    : m_camera(camera)
    , m_inputManager(inputManager)
    , m_input(input)
    , m_editorUI(editorUI)
{
}

void CameraController::Update(float dt)
{
    // ★ ОТЛАДКА 1: Проверяем, вызывается ли сам метод
    static int callCounter = 0;
    if (++callCounter % 30 == 0)
    {
        OutputDebugStringA("✅ CameraController::Update called\n");
    }

    dt = min(dt, 0.05f);

    // ★ ОТЛАДКА 2: Проверяем параметры
    if (callCounter % 30 == 0)
    {
        char buf[256];
        sprintf_s(buf, "dt=%.6f, INPUT_MOVE_SPEED=%.2f\n", dt, Settings::INPUT_MOVE_SPEED);
        OutputDebugStringA(buf);
    }

    const bool rightDown = m_inputManager->IsCameraMode();

    if (rightDown && !m_inputManager->IsMouseCaptured(m_input) && !m_editorUI->WantCaptureMouse())
        m_inputManager->SetMouseCapture(m_input, true);
    else if (!rightDown && m_inputManager->IsMouseCaptured(m_input))
        m_inputManager->SetMouseCapture(m_input, false);

    // Вращение мышью
    if (m_inputManager->IsMouseCaptured(m_input))
    {
        int deltaX, deltaY;
        m_input->GetMouseDelta(deltaX, deltaY);
        m_inputManager->ResetMouseDelta(m_input);

        float rotX = m_camera->GetRotation().x + deltaY * Settings::CAMERA_SENSITIVITY;
        float rotY = m_camera->GetRotation().y + deltaX * Settings::CAMERA_SENSITIVITY;
        rotX = clamp(rotX, -89.f, 89.f);
        m_camera->SetRotation(rotX, rotY, m_camera->GetRotation().z);
    }

    // Движение WASDRF
    XMFLOAT3 oldPos = m_camera->GetPosition();  // ★ Сохраняем старую позицию
    float posX = oldPos.x;
    float posY = oldPos.y;
    float posZ = oldPos.z;

    float speed = Settings::INPUT_MOVE_SPEED;
    if (m_inputManager->IsKeyDown(m_input, VK_SHIFT))   speed *= 2.f;
    if (m_inputManager->IsKeyDown(m_input, VK_CONTROL)) speed *= 0.5f;

    const float yawRad = XMConvertToRadians(m_camera->GetRotation().y);
    const float sinYaw = sinf(yawRad), cosYaw = cosf(yawRad);

    // ★ ОТЛАДКА 3: Проверяем каждую клавишу ОТДЕЛЬНО
    bool wPressed = m_inputManager->IsKeyDown(m_input, 'W');
    if (wPressed)
    {
        OutputDebugStringA("WWWWWWWWWWWWWWWWWWWWWWWWWWWWW");
    }
    m_input->DumpKeyState('W');

    bool aPressed = m_inputManager->IsKeyDown(m_input, 'A');
    bool sPressed = m_inputManager->IsKeyDown(m_input, 'S');
    bool dPressed = m_inputManager->IsKeyDown(m_input, 'D');
    bool rPressed = m_inputManager->IsKeyDown(m_input, 'R');
    bool fPressed = m_inputManager->IsKeyDown(m_input, 'F');
    bool shiftPressed = m_inputManager->IsKeyDown(m_input, VK_SHIFT);
    bool ctrlPressed = m_inputManager->IsKeyDown(m_input, VK_CONTROL);

    if (callCounter % 10 == 0)
    {
        char buf[512];
        sprintf_s(buf, "Keys: W=%d A=%d S=%d D=%d R=%d F=%d | SHIFT=%d CTRL=%d | Speed=%.2f\n",
            wPressed, aPressed, sPressed, dPressed, rPressed, fPressed, shiftPressed, ctrlPressed, speed);
        OutputDebugStringA(buf);
    }

    if (!wPressed)
    {
        posX += sinYaw * speed * dt;
        posZ += cosYaw * speed * dt;
        OutputDebugStringA("→ W: moving forward\n");
    }
    if (sPressed)
    {
        posX -= sinYaw * speed * dt;
        posZ -= cosYaw * speed * dt;
        OutputDebugStringA("→ S: moving backward\n");
    }
    if (aPressed)
    {
        posX -= cosYaw * speed * dt;
        posZ += sinYaw * speed * dt;
        OutputDebugStringA("→ A: moving left\n");
    }
    if (dPressed)
    {
        posX += cosYaw * speed * dt;
        posZ -= sinYaw * speed * dt;
        OutputDebugStringA("→ D: moving right\n");
    }
    if (rPressed) posY += speed * dt;
    if (fPressed) posY -= speed * dt;

    // ★ ОТЛАДКА 4: Проверяем, изменилась ли позиция
    bool posChanged = (oldPos.x != posX) || (oldPos.y != posY) || (oldPos.z != posZ);
    if (posChanged)
    {
        char buf[512];
        sprintf_s(buf, "Position CHANGED: (%.2f, %.2f, %.2f) → (%.2f, %.2f, %.2f)\n",
            oldPos.x, oldPos.y, oldPos.z, posX, posY, posZ);
        OutputDebugStringA(buf);
    }

    // ★ ОТЛАДКА 5: Вызываем SetPosition и проверяем результат
    m_camera->SetPosition(posX, posY, posZ);

    XMFLOAT3 newPos = m_camera->GetPosition();
    if (newPos.x != posX || newPos.y != posY || newPos.z != posZ)
    {
        char buf[512];
        sprintf_s(buf, "⚠️ WARNING: SetPosition DIDN'T WORK!\n");
        sprintf_s(buf, "   Requested: (%.2f, %.2f, %.2f)\n", posX, posY, posZ);
        sprintf_s(buf, "   Actual:    (%.2f, %.2f, %.2f)\n", newPos.x, newPos.y, newPos.z);
        OutputDebugStringA(buf);
    }
}