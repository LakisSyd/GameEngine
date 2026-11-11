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

void CameraController::SetFirstPersonEyeHeight(float value)
{
    m_firstPersonEyeHeight = std::clamp(value, 0.1f, 3.0f);
}

void CameraController::SetFirstPersonEyeRightOffset(float value)
{
    m_firstPersonEyeRightOffset = std::clamp(value, -1.0f, 1.0f);
}

void CameraController::SetFirstPersonForwardOffset(float value)
{
    m_firstPersonForwardOffset = std::clamp(value, -1.0f, 1.0f);
}

void CameraController::SetThirdPersonDistance(float value)
{
    m_followDistance = std::clamp(value, 1.0f, 30.0f);
}

void CameraController::SetThirdPersonTargetOffset(const XMFLOAT3& value)
{
    m_thirdPersonTargetOffset = {
        std::clamp(value.x, -2.0f, 2.0f),
        std::clamp(value.y, 0.1f, 3.0f),
        std::clamp(value.z, -2.0f, 2.0f) };
}

void CameraController::SetPlayViewMode(PlayViewMode mode)
{
    m_playViewMode = mode;
    const float minPitch = IsFirstPerson() ? m_firstPersonMinPitch : m_followMinPitch;
    const float maxPitch = IsFirstPerson() ? m_firstPersonMaxPitch : m_followMaxPitch;
    m_followPitch = std::clamp(m_followPitch, minPitch, maxPitch);
    if (IsFirstPerson())
        m_socketStudioPitch = std::clamp(
            m_socketStudioPitch, minPitch, maxPitch);
}

void CameraController::SetFollowPitch(float value)
{
    m_followPitch = IsFirstPerson()
        ? std::clamp(value, m_firstPersonMinPitch, m_firstPersonMaxPitch)
        : std::clamp(value, m_followMinPitch, m_followMaxPitch);
}

void CameraController::SetLookUpLimit(float degrees)
{
    m_followMinPitch = -std::clamp(degrees, 0.0f, 89.0f);
    if (!IsFirstPerson())
        m_followPitch = std::clamp(m_followPitch, m_followMinPitch, m_followMaxPitch);
}

void CameraController::SetLookDownLimit(float degrees)
{
    m_followMaxPitch = std::clamp(degrees, 0.0f, 89.0f);
    if (!IsFirstPerson())
        m_followPitch = std::clamp(m_followPitch, m_followMinPitch, m_followMaxPitch);
}

void CameraController::SetFirstPersonLookUpLimit(float degrees)
{
    m_firstPersonMinPitch = -std::clamp(degrees, 0.0f, 89.0f);
    if (IsFirstPerson())
    {
        m_followPitch = std::clamp(
            m_followPitch, m_firstPersonMinPitch, m_firstPersonMaxPitch);
        m_socketStudioPitch = std::clamp(
            m_socketStudioPitch, m_firstPersonMinPitch, m_firstPersonMaxPitch);
    }
}

void CameraController::SetFirstPersonLookDownLimit(float degrees)
{
    m_firstPersonMaxPitch = std::clamp(degrees, 0.0f, 89.0f);
    if (IsFirstPerson())
    {
        m_followPitch = std::clamp(
            m_followPitch, m_firstPersonMinPitch, m_firstPersonMaxPitch);
        m_socketStudioPitch = std::clamp(
            m_socketStudioPitch, m_firstPersonMinPitch, m_firstPersonMaxPitch);
    }
}

void CameraController::SetSocketStudioEnabled(bool enabled)
{
    if (enabled && !m_socketStudioEnabled)
    {
        m_socketStudioYaw = m_followYaw;
        m_socketStudioPitch = m_followPitch;
        m_socketStudioDistance = (std::max)(0.35f, m_followDistance);
        m_socketStudioTargetOffset = { 0.0f, 1.4f, 0.0f };
    }
    m_socketStudioEnabled = enabled;
}

void CameraController::SetSocketStudioFreeFly(bool enabled)
{
    if (m_socketStudioFreeFly == enabled) return;
    m_socketStudioFreeFly = enabled;
    ReleaseMouseCapture();
}

void CameraController::SetSocketStudioPitch(float degrees)
{
    m_socketStudioPitch = IsFirstPerson()
        ? std::clamp(degrees, m_firstPersonMinPitch, m_firstPersonMaxPitch)
        : std::clamp(degrees, -85.0f, 85.0f);
}

void CameraController::SetSocketStudioDistance(float distance)
{
    m_socketStudioDistance = std::clamp(distance, 0.35f, 15.0f);
}

float CameraController::GetFollowYaw() const
{
    return m_followYaw;
}

void CameraController::UpdateFollowLook(float dt, bool inputBlockedByUi)
{
    if (!m_inputManager || !m_input)
        return;

    if (m_socketStudioFreeFly)
    {
        // Observer/Studio uses the same mouse contract as the editor camera:
        // the cursor remains available for ImGui, RMB outside UI captures it
        // only while looking around, and WASD + R/F move the free camera.
        UpdateFree(dt, inputBlockedByUi);
        return;
    }

    if (inputBlockedByUi)
    {
        // An open device panel owns the cursor instead of camera look.
        ReleaseMouseCapture();
    }
    else
    {
        if (!m_inputManager->IsMouseCaptured(m_input))
            m_inputManager->SetMouseCapture(m_input, true);

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
    }

    SetFollowPitch(m_followPitch);
}

void CameraController::UpdateFollow(const TransformComponent* targetTransform)
{
    if (m_socketStudioFreeFly) return;
    if (!m_camera || !targetTransform)
        return;

    // The character transform is already interpolated between physics steps.
    // Keep the orbit at the current input angle: smoothing the camera's world
    // position while turning made it trail around a different pivot than the
    // character and caused visible lateral drift.
    using namespace DirectX;

    const float viewYaw = m_socketStudioEnabled ? m_socketStudioYaw : m_followYaw;
    const float viewPitch = m_socketStudioEnabled ? m_socketStudioPitch : m_followPitch;
    const float distance = m_socketStudioEnabled ? m_socketStudioDistance : m_followDistance;
    const float yawRad = XMConvertToRadians(viewYaw);
    const float pitchRad = XMConvertToRadians(viewPitch);

    XMFLOAT3 target = targetTransform->position;

    if (m_socketStudioEnabled)
    {
        target.x += m_socketStudioTargetOffset.x;
        target.y += m_socketStudioTargetOffset.y;
        target.z += m_socketStudioTargetOffset.z;
    }
    else
    {
        target.x += cosf(yawRad) * m_thirdPersonTargetOffset.x +
            sinf(yawRad) * m_thirdPersonTargetOffset.z;
        target.y += m_thirdPersonTargetOffset.y;
        target.z += -sinf(yawRad) * m_thirdPersonTargetOffset.x +
            cosf(yawRad) * m_thirdPersonTargetOffset.z;
    }

    const float cosPitch = cosf(pitchRad);
    const float sinPitch = sinf(pitchRad);
    const float sinYaw = sinf(yawRad);
    const float cosYaw = cosf(yawRad);

    if (m_playViewMode == PlayViewMode::FirstPerson)
    {
        XMFLOAT3 eye = targetTransform->position;
        eye.y += m_firstPersonEyeHeight;
        eye.x += cosYaw * m_firstPersonEyeRightOffset +
            sinYaw * m_firstPersonForwardOffset;
        eye.z += -sinYaw * m_firstPersonEyeRightOffset +
            cosYaw * m_firstPersonForwardOffset;

        m_camera->SetPosition(eye.x, eye.y, eye.z);
        m_camera->SetRotation(viewPitch, viewYaw, 0.0f);
        return;
    }

    XMFLOAT3 desired =
    {
        target.x - sinYaw * cosPitch * distance,
        target.y + sinPitch * distance,
        target.z - cosYaw * cosPitch * distance
    };

    m_camera->SetPosition(desired.x, desired.y, desired.z);
    m_camera->SetRotation(viewPitch, viewYaw, 0.0f);
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
    m_mode = Mode::FollowTarget;

    if (m_inputManager && m_input)
        m_inputManager->SetMouseCapture(m_input, true);
}

void CameraController::ExitPlayMode()
{
    m_socketStudioEnabled = false;
    m_socketStudioFreeFly = false;
    ReleaseMouseCapture();
}
