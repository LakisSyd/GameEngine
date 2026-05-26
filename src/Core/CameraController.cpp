#include "CameraController.h"

#include <algorithm>
#include <cmath>

#include "cameraclass.h"
#include "Components.h"
#include "GameSettings.h"
#include "InputClass.h"
#include "InputManager.h"
#include "Components.h"
using namespace DirectX;

void CameraController::SetInput(InputManager* inputManager, InputClass* input, HWND hwnd)
{
    m_inputManager = inputManager;
    m_input = input;
    m_hwnd = hwnd;
}

void CameraController::UpdateModeToggle(bool inputBlockedByUi)
{
    if (!m_inputManager || !m_input)
        return;

    const bool toggleDown = m_inputManager->IsKeyDown(m_input, 'C');
    if (toggleDown && !m_toggleWasDown && !inputBlockedByUi)
    {
        m_mode = IsFreeMode() ? Mode::FollowTarget : Mode::Free;
        ReleaseMouseCapture();
    }

    m_toggleWasDown = toggleDown;
}

void CameraController::UpdateFree(float dt, bool inputBlockedByUi)
{
    if (inputBlockedByUi)
    {
        ReleaseMouseCapture();
        return;
    }

    ApplyFreeLook(dt);
    ApplyFreeMove(dt);
}

float CameraController::GetFollowYaw() const
{
    return m_followYaw;
}

void CameraController::UpdateFollow(float dt, const TransformComponent* targetTransform)
{
    if (!m_camera || !targetTransform || !m_inputManager || !m_input)
        return;

    dt = std::min(dt, 0.05f);

    // В PlayMode мышь всегда захвачена
    if (!m_inputManager->IsMouseCaptured(m_input))
    {
        m_inputManager->SetMouseCapture(m_input, true);
    }

    m_input->UpdateCapturedMouseDelta(m_hwnd);

    int dx = 0;
    int dy = 0;
    m_input->GetMouseDelta(dx, dy);
    m_inputManager->ResetMouseDelta(m_input);

    dx = std::clamp(dx, -200, 200);
    dy = std::clamp(dy, -200, 200);

    const float sensitivity = Settings::CAMERA_SENSITIVITY;

    m_followYaw += dx * sensitivity;
    m_followPitch += dy * sensitivity;

    m_followPitch = std::clamp(
        m_followPitch,
        m_followMinPitch,
        m_followMaxPitch
    );
    
   

    using namespace DirectX;

    const float yawRad = XMConvertToRadians(m_followYaw);
    const float pitchRad = XMConvertToRadians(m_followPitch);

    XMFLOAT3 target = targetTransform->position;

    // Небольшая высота точки, в которую смотрим
    target.y += 1.5f;

    const float cosPitch = cosf(pitchRad);
    const float sinPitch = sinf(pitchRad);
    const float sinYaw = sinf(yawRad);
    const float cosYaw = cosf(yawRad);

    XMFLOAT3 desired =
    {
        target.x - sinYaw * cosPitch * m_followDistance,
        target.y + sinPitch * m_followDistance,
        target.z - cosYaw * cosPitch * m_followDistance
    };

    XMFLOAT3 current = m_camera->GetPosition();

    const float followSpeed = 12.0f;
    const float t = std::clamp(followSpeed * dt, 0.0f, 1.0f);

    XMFLOAT3 next =
    {
        current.x + (desired.x - current.x) * t,
        current.y + (desired.y - current.y) * t,
        current.z + (desired.z - current.z) * t
    };
        
    m_camera->SetPosition(next.x, next.y, next.z);

    XMVECTOR cameraPos = XMLoadFloat3(&next);
    XMVECTOR targetPos = XMLoadFloat3(&target);
    XMVECTOR direction = XMVector3Normalize(
        XMVectorSubtract(targetPos, cameraPos)
    );

    XMFLOAT3 dir;
    XMStoreFloat3(&dir, direction);

    const float yaw =
        XMConvertToDegrees(std::atan2(dir.x, dir.z));

    const float pitch =
        XMConvertToDegrees(std::asin(std::clamp(-dir.y, -1.0f, 1.0f)));

    m_camera->SetRotation(pitch, yaw, 0.0f);
}



void CameraController::ReleaseMouseCapture()
{
    if (!m_inputManager || !m_input)
        return;

    if (!m_inputManager->IsMouseCaptured(m_input))
        return;

    m_inputManager->SetMouseCapture(m_input, false);
    m_inputManager->ResetMouseDelta(m_input);
}

void CameraController::ApplyFreeLook(float /*dt*/)
{
    if (!m_camera || !m_inputManager || !m_input)
        return;

    const bool rightDown = m_inputManager->IsCameraMode();
    if (rightDown && !m_inputManager->IsMouseCaptured(m_input))
    {
        m_inputManager->SetMouseCapture(m_input, true);
    }
    else if (!rightDown)
    {
        ReleaseMouseCapture();
    }

    if (!m_inputManager->IsMouseCaptured(m_input))
        return;

    m_input->UpdateCapturedMouseDelta(m_hwnd);

    int deltaX = 0;
    int deltaY = 0;
    m_input->GetMouseDelta(deltaX, deltaY);
    m_inputManager->ResetMouseDelta(m_input);

    deltaX = std::clamp(deltaX, -200, 200);
    deltaY = std::clamp(deltaY, -200, 200);

    const float mouseSensitivity = Settings::CAMERA_SENSITIVITY;
    XMFLOAT3 rotation = m_camera->GetRotation();
    float rotX = rotation.x + deltaY * mouseSensitivity;
    float rotY = rotation.y + deltaX * mouseSensitivity;
    rotX = std::clamp(rotX, -89.0f, 89.0f);
    m_camera->SetRotation(rotX, rotY, rotation.z);
}

void CameraController::ApplyFreeMove(float dt)
{
    if (!m_camera || !m_inputManager || !m_input)
        return;

    dt = std::min(dt, 0.05f);

    XMFLOAT3 position = m_camera->GetPosition();

    float speed = Settings::INPUT_MOVE_SPEED;
    if (m_inputManager->IsKeyDown(m_input, VK_SHIFT))   speed *= 2.0f;
    if (m_inputManager->IsKeyDown(m_input, VK_CONTROL)) speed *= 0.5f;

    const float yawRad = XMConvertToRadians(m_camera->GetRotation().y);
    const float sinYaw = std::sin(yawRad);
    const float cosYaw = std::cos(yawRad);

	

    if (m_inputManager->IsKeyDown(m_input, 'W')) { position.x += sinYaw * speed * dt; position.z += cosYaw * speed * dt; }
    if (m_inputManager->IsKeyDown(m_input, 'S')) { position.x -= sinYaw * speed * dt; position.z -= cosYaw * speed * dt; }
    if (m_inputManager->IsKeyDown(m_input, 'A')) { position.x -= cosYaw * speed * dt; position.z += sinYaw * speed * dt; }
    if (m_inputManager->IsKeyDown(m_input, 'D')) { position.x += cosYaw * speed * dt; position.z -= sinYaw * speed * dt; }
    if (m_inputManager->IsKeyDown(m_input, 'R')) position.y += speed * dt;
    if (m_inputManager->IsKeyDown(m_input, 'F')) position.y -= speed * dt;

    m_camera->SetPosition(position.x, position.y, position.z);
}
void CameraController::EnterPlayMode()
{
    if (m_inputManager && m_input)
        m_inputManager->SetMouseCapture(m_input, true);
}

void CameraController::ExitPlayMode()
{
    ReleaseMouseCapture();
}