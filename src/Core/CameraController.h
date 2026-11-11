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
    void SetPlayViewMode(PlayViewMode mode);
    bool IsFirstPerson() const { return m_playViewMode == PlayViewMode::FirstPerson; }

    float GetFirstPersonEyeHeight() const { return m_firstPersonEyeHeight; }
    void SetFirstPersonEyeHeight(float value);
    float GetFirstPersonEyeRightOffset() const { return m_firstPersonEyeRightOffset; }
    void SetFirstPersonEyeRightOffset(float value);
    float GetFirstPersonForwardOffset() const { return m_firstPersonForwardOffset; }
    void SetFirstPersonForwardOffset(float value);

    float GetThirdPersonDistance() const { return m_followDistance; }
    void SetThirdPersonDistance(float value);
    DirectX::XMFLOAT3 GetThirdPersonTargetOffset() const { return m_thirdPersonTargetOffset; }
    void SetThirdPersonTargetOffset(const DirectX::XMFLOAT3& value);
    void SetFollowYaw(float value) { m_followYaw = value; }
    float GetFollowPitch() const { return m_followPitch; }
    void SetFollowPitch(float value);
    float GetLookUpLimit() const { return -m_followMinPitch; }
    void SetLookUpLimit(float degrees);
    float GetLookDownLimit() const { return m_followMaxPitch; }
    void SetLookDownLimit(float degrees);
    float GetFirstPersonLookUpLimit() const { return -m_firstPersonMinPitch; }
    void SetFirstPersonLookUpLimit(float degrees);
    float GetFirstPersonLookDownLimit() const { return m_firstPersonMaxPitch; }
    void SetFirstPersonLookDownLimit(float degrees);

    void SetSocketStudioEnabled(bool enabled);
    void SetSocketStudioFreeFly(bool enabled);
    bool IsSocketStudioFreeFly() const { return m_socketStudioFreeFly; }
    bool IsSocketStudioEnabled() const { return m_socketStudioEnabled; }
    float GetSocketStudioYaw() const { return m_socketStudioYaw; }
    void SetSocketStudioYaw(float degrees) { m_socketStudioYaw = degrees; }
    float GetSocketStudioPitch() const { return m_socketStudioPitch; }
    void SetSocketStudioPitch(float degrees);
    float GetSocketStudioDistance() const { return m_socketStudioDistance; }
    void SetSocketStudioDistance(float distance);
    DirectX::XMFLOAT3 GetSocketStudioTargetOffset() const { return m_socketStudioTargetOffset; }
    void SetSocketStudioTargetOffset(const DirectX::XMFLOAT3& offset) { m_socketStudioTargetOffset = offset; }

    void UpdateModeToggle(bool inputBlockedByUi);
    void UpdateFree(float dt, bool inputBlockedByUi);
    float GetFollowYaw() const;
    void UpdateFollowLook(float dt, bool inputBlockedByUi = false);
    void UpdateFollow(const TransformComponent* targetTransform);
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
    PlayViewMode m_playViewMode = PlayViewMode::ThirdPerson;
    bool m_toggleWasDown = false;
    DirectX::XMFLOAT3 m_followOffset = { 0.0f, 6.0f, -10.0f };

    float m_followYaw = 0.0f;
    float m_followPitch = 10.0f;
    float m_followDistance = 4.0f;
    float m_firstPersonEyeHeight = 1.6f;
    float m_firstPersonEyeRightOffset = 0.0f;
    float m_firstPersonForwardOffset = 0.15f;
    DirectX::XMFLOAT3 m_thirdPersonTargetOffset = { 0.0f, 1.5f, 0.0f };

    float m_followMinPitch = -20.0f;
    float m_followMaxPitch = 70.0f;
    float m_firstPersonMinPitch = -65.0f;
    float m_firstPersonMaxPitch = 70.0f;
    bool m_socketStudioEnabled = false;
    bool m_socketStudioFreeFly = false;
    float m_socketStudioYaw = 0.0f;
    float m_socketStudioPitch = 10.0f;
    float m_socketStudioDistance = 2.2f;
    DirectX::XMFLOAT3 m_socketStudioTargetOffset = { 0.0f, 1.4f, 0.0f };
};
