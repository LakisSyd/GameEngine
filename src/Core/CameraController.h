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

    enum class PlayViewMode
    {
        ThirdPerson,
        FirstPerson
    };

    void SetCamera(CameraClass* camera) { m_camera = camera; }
    void SetInput(InputManager* inputManager, InputClass* input, HWND hwnd);

    Mode GetMode() const { return m_mode; }
    bool IsFreeMode() const { return m_mode == Mode::Free; }
    bool IsFollowMode() const { return m_mode == Mode::FollowTarget; }

    void SetFollowOffset(const DirectX::XMFLOAT3& offset) { m_followOffset = offset; }
    DirectX::XMFLOAT3 GetFollowOffset() const { return m_followOffset; }

    PlayViewMode GetPlayViewMode() const { return m_playViewMode; }
    void SetPlayViewMode(PlayViewMode mode) { m_playViewMode = mode; }
    bool IsFirstPerson() const { return m_playViewMode == PlayViewMode::FirstPerson; }

    float GetFirstPersonEyeHeight() const { return m_firstPersonEyeHeight; }
    void SetFirstPersonEyeHeight(float value);
    float GetFirstPersonForwardOffset() const { return m_firstPersonForwardOffset; }
    void SetFirstPersonForwardOffset(float value);

    float GetThirdPersonDistance() const { return m_followDistance; }
    void SetThirdPersonDistance(float value);
    float GetFollowPitch() const { return m_followPitch; }
    void SetFollowPitch(float value);
    float GetLookUpLimit() const { return -m_followMinPitch; }
    void SetLookUpLimit(float degrees);
    float GetLookDownLimit() const { return m_followMaxPitch; }
    void SetLookDownLimit(float degrees);

    void UpdateModeToggle(bool inputBlockedByUi);
    void UpdateFree(float dt, bool inputBlockedByUi);
    float GetFollowYaw() const;
    void UpdateFollow(
        float dt,
        const TransformComponent* targetTransform,
        bool inputBlockedByUi = false);
    void ReleaseMouseCapture();
    void EnterPlayMode();

    void ExitPlayMode();

private:
    void ApplyFreeLook(float dt);
    void ApplyFreeMove(float dt);

 

private:
    CameraClass* m_camera = nullptr;
    InputManager* m_inputManager = nullptr;
    InputClass* m_input = nullptr;
    HWND m_hwnd = nullptr;

    Mode m_mode = Mode::FollowTarget;
    PlayViewMode m_playViewMode = PlayViewMode::FirstPerson;
    bool m_toggleWasDown = false;
    DirectX::XMFLOAT3 m_followOffset = { 0.0f, 6.0f, -10.0f };

    float m_followYaw = 0.0f;
    float m_followPitch = 20.0f;
    float m_followDistance = 8.0f;
    float m_firstPersonEyeHeight = 1.6f;
    float m_firstPersonForwardOffset = 0.15f;

    float m_followMinPitch = -20.0f;
    float m_followMaxPitch = 70.0f;
};
