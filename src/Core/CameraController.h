#pragma once

#include <DirectXMath.h>
#include <windows.h>

class CameraClass;
class InputClass;
class InputManager;
struct TransformComponent;

class CameraController
{
public:
    enum class Mode
    {
        Free,
        FollowTarget
    };

    void SetCamera(CameraClass* camera) { m_camera = camera; }
    void SetInput(InputManager* inputManager, InputClass* input, HWND hwnd);

    Mode GetMode() const { return m_mode; }
    bool IsFreeMode() const { return m_mode == Mode::Free; }
    bool IsFollowMode() const { return m_mode == Mode::FollowTarget; }

    void SetFollowOffset(const DirectX::XMFLOAT3& offset) { m_followOffset = offset; }
    DirectX::XMFLOAT3 GetFollowOffset() const { return m_followOffset; }

    void UpdateModeToggle(bool inputBlockedByUi);
    void UpdateFree(float dt, bool inputBlockedByUi);
    void UpdateFollow(float dt, const TransformComponent* targetTransform);
    void ReleaseMouseCapture();

private:
    void ApplyFreeLook(float dt);
    void ApplyFreeMove(float dt);

private:
    CameraClass* m_camera = nullptr;
    InputManager* m_inputManager = nullptr;
    InputClass* m_input = nullptr;
    HWND m_hwnd = nullptr;

    Mode m_mode = Mode::FollowTarget;
    bool m_toggleWasDown = false;
    DirectX::XMFLOAT3 m_followOffset = { 0.0f, 6.0f, -10.0f };
};
