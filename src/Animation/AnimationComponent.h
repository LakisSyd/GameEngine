#pragma once
#include "ECSTypes.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <string>

enum class AnimationState
{
    Idle = 0,
    Walk = 1,
    Run = 2,
    RifleIdle = 3,
    RifleGun = 4,
    RifleRun = 5,
    RifleWalk = 6,
    RifleHipIdle = 7,
    Punch = 8,
    PistolIdle = 9,
    PistolWalk = 10,
    PistolShot = 11,
    CrouchIdle = 12,
    CrouchWalk = 13,
    CrouchPistolIdle = 14,
    CrouchPistolWalk = 15,
    CrouchRifleIdle = 16,
    CrouchRifleWalk = 17,
    Search1 = 18,
    Search2 = 19,
    Search3 = 20,
    RiflePeekLeft = 21,
};

struct AnimationBinding
{
    AnimationState state;
    std::string name;
    int index = -1;
};

struct AnimationPoseKey
{
    std::string clip;
    std::string bone;
    int variant = -1; // -1: общий ключ; 0 и выше: конкретный вариант удара.
    float time = 0.0f;
    DirectX::XMFLOAT3 position = {};
    DirectX::XMFLOAT3 rotation = {}; // Углы в градусах, добавляются к исходной анимации.
};

// Virtual Animation Studio controls. They are deliberately not real skeleton
// nodes: translating a deform bone changes its local length and stretches the
// skin. The hand target controls reach, while the elbow pole controls the bend
// plane; both are solved by rotating joints and preserving segment lengths.
inline constexpr const char* kAnimationLeftHandIKControl = "@CTRL_Hand_IK.L";
inline constexpr const char* kAnimationRightHandIKControl = "@CTRL_Hand_IK.R";
inline constexpr const char* kAnimationLeftElbowPoleControl = "@CTRL_Elbow_Pole.L";
inline constexpr const char* kAnimationRightElbowPoleControl = "@CTRL_Elbow_Pole.R";
inline constexpr const char* kAnimationHeadLookIKControl = "@CTRL_Head_Look";

inline bool IsAnimationHandIKControl(const std::string& name)
{
    return name == kAnimationLeftHandIKControl ||
        name == kAnimationRightHandIKControl;
}

inline bool IsRightAnimationHandIKControl(const std::string& name)
{
    return name == kAnimationRightHandIKControl;
}

inline bool IsAnimationElbowPoleControl(const std::string& name)
{
    return name == kAnimationLeftElbowPoleControl ||
        name == kAnimationRightElbowPoleControl;
}

inline bool IsRightAnimationElbowPoleControl(const std::string& name)
{
    return name == kAnimationRightElbowPoleControl;
}

inline bool IsAnimationArmIKControl(const std::string& name)
{
    return IsAnimationHandIKControl(name) ||
        IsAnimationElbowPoleControl(name);
}

inline bool IsAnimationHeadIKControl(const std::string& name)
{
    return name == kAnimationHeadLookIKControl;
}

inline bool IsAnimationIKControl(const std::string& name)
{
    return IsAnimationArmIKControl(name) || IsAnimationHeadIKControl(name);
}

// Virtual counterpart of Blender's CTRL_Fist.L / CTRL_Fist.R. The authored
// control is baked out of the game GLB, so the engine keeps one scalar curve
// and applies the captured Blender finger rotations at runtime.
struct AnimationFistKey
{
    std::string clip;
    int variant = -1;
    float time = 0.0f;
    bool rightHand = false;
    float amount = 0.0f; // -1 opens, 0 keeps the imported clip, 1 closes.
};

// Camera-space adjustment for one first-person clip. Variant -1 is shared;
// numbered variants may use the same GLB clip with different framing.
struct FirstPersonClipOffset
{
    std::string clip;
    int variant = -1;
    DirectX::XMFLOAT3 position = {};
    DirectX::XMFLOAT3 rotation = {}; // Degrees, added to the base view rotation.
};

struct AnimationClipSpeed
{
    std::string clip;
    float speed = 1.0f;
};

struct PunchVariant
{
    std::string name;
    // Эти клипы могут называться одинаково, потому что ищутся в разных GLB:
    // worldClip — в полной модели, firstPersonClip — в модели рук.
    std::string worldClip; // Пустое значение использует обычный Punch binding.
    std::string firstPersonClip; // Проигрывается только на FirstPersonView.
    bool rightHand = false;
    float contactPhase = -1.0f; // Отрицательное значение использует общую настройку игрока.
    float weight = 1.0f; // Относительный шанс выбора; 0 исключает вариант.
    // Runtime punch-IK tuning. Strength < 0 inherits the owner default
    // (PlayerComponent for the player, full correction for an NPC).
    float ikStrength = -1.0f;
    // Local contact point measured from the wrist and an additive wrist
    // rotation applied after the arm reaches the IK target.
    DirectX::XMFLOAT3 fistOffset = {};
    DirectX::XMFLOAT3 handRotation = {}; // Degrees.
    // Aim correction uses the current aim basis: X is sideways, Y is up,
    // and Z moves the target along the punch direction.
    DirectX::XMFLOAT3 aimOffset = {};
    // Explicit model-space elbow bend direction measured from the shoulder.
    // Zero preserves the bend plane authored by the animation. Only the
    // direction matters; the solver normalizes the vector before using it.
    DirectX::XMFLOAT3 elbowPoleOffset = {};
    float armExtension = 0.92f; // Fraction of total arm length used by IK.
    // Normalized clip interval in which punch IK is allowed to affect the arm.
    // The correction fades from zero at start to full at contact, then back to
    // zero at end. Narrowing this window keeps the authored wind-up/recovery.
    float ikStartPhase = 0.0f;
    float ikEndPhase = 1.0f;
};

struct MeleeActionConfig
{
    bool guardEnabled = true;
    std::string guardWorldClip = "Kevin_Melee_Guard";
    std::string guardFirstPersonClip = "Kevin_Melee_Guard";
    // Optional full-body movement clip; empty keeps locomotion plus guard arms.
    std::string guardMoveWorldClip;
    std::string guardMoveFirstPersonClip;
    // Full-body clip below the upper-body punch. Empty uses guardWorldClip
    // for planted fighting feet, for both players and unarmed NPCs.
    std::string attackBaseWorldClip;
    bool attackRequiresGuard = true;
};


struct AnimationComponent
{
    AnimationState currentState = AnimationState::Idle;

    int animationIndex = 0;
    float currentTime = 0.0f;
    bool isPlaying = true;
    // Animation stance only. Capsule resizing belongs to the movement system.
    bool crouchPose = false;
    // Поиск: выбранный слот и остаток времени клипа. Это временные данные,
    // а имена трёх клипов хранятся в обычных AnimationBinding ниже.
    int searchAnimationSlot = -1;
    float searchAnimationTimeRemaining = 0.0f;
    bool searchAnimationActive = false;

    // Основная анимация тела и слой рук имеют независимые таймеры.
    int upperBodyAnimationIndex = -1;
    float upperBodyTime = 0.0f;
    std::string upperBodyRoot = "mixamorig:Spine";
    // Состояние воспроизведения выстрела. Удержание кнопки не должно зацикливать
    // отдачу между настоящими выстрелами и оставлять персонажа в позе стрельбы.
    Entity rifleAnimationWeapon = INVALID_ENTITY;
    unsigned int observedShotsFired = 0;
    float rifleShotTimeRemaining = 0.0f;

    // Одноразовая анимация, запрошенная игровой логикой: удар, перезарядка,
    // реакция на попадание и т.п. Контроллер принимает запрос один раз и
    // удерживает клип до завершения, не давая анимациям ожидания или ходьбы
    // его перезаписать.
    AnimationState oneShotState = AnimationState::Idle;
    bool oneShotRequested = false;
    float oneShotTimeRemaining = 0.0f;
    bool leftHandIKApplied = false;
    float leftHandIKError = 0.0f;
    // Runtime-only pose inspection for the socket studio.
    bool socketPreviewEnabled = false;
    bool socketPreviewPlaying = false;
    bool socketPreviewApplyIK = true;
    // Preview a world/3P clip on the first-person arms by matching its name
    // and phase, then apply the FP component's own pose keys and framing.
    bool socketPreviewFirstPersonFromWorld = false;
    bool socketPreviewPunch = false;
    bool socketPreviewApplyPunchIK = false;
    int socketPreviewPunchVariant = 0;
    int socketPreviewPunchMovement = 0; // Automatic fallback: 0 Idle, 1 Walk, 2 Run.
    bool socketPreviewPunchUseCameraAim = false;
    float socketPreviewPunchAimDistance = 1.25f;
    float socketPreviewPunchAimError = 0.0f;
    // Animation Studio can reproduce the runtime lower-body + upper-body
    // composition while authoring additive keys on either source clip.
    bool socketPreviewBlendEnabled = false;
    int socketPreviewUpperClip = -1;
    float socketPreviewUpperTime = 0.0f;
    bool socketPreviewSyncLayerPhase = true;
    int socketPreviewClip = 0; // Индекс любого импортированного клипа модели.
    float socketPreviewTime = 0.0f;
    std::vector<AnimationPoseKey> poseKeys;
    std::vector<AnimationFistKey> fistKeys;
    std::vector<FirstPersonClipOffset> firstPersonClipOffsets;
    std::vector<AnimationClipSpeed> clipSpeeds;
    int poseVariant = -1;
    std::string profileName; // assets/config/animations/<name>.json, read on scene load.
    MeleeActionConfig meleeActions;
    std::vector<PunchVariant> punchVariants = {
        { "Left jab", "Kevin_Melee_Jab_L", "Kevin_Melee_Jab_L", false, -1.0f, 1.0f },
        { "Right cross", "Kevin_Melee_Cross_R", "Kevin_Melee_Cross_R", true, -1.0f, 1.0f } };

    std::vector<AnimationBinding> animations =
    {
        { AnimationState::Idle, "Idle" },
        { AnimationState::Walk, "Walk" },
        { AnimationState::Run, "Run" },
        { AnimationState::RifleIdle, "" },
        { AnimationState::RifleGun, "" },
        { AnimationState::RifleRun, "Kevin_Rifle_Run" },
        { AnimationState::RifleWalk, "Kevin_Walk_R_I" },
        { AnimationState::RifleHipIdle, "Kevin_Idle_Gun_R_SoftSway" },
        { AnimationState::Punch, "Kevin_Punch.1" },
        { AnimationState::PistolIdle, "Kevin_Pistol_Idle" },
        { AnimationState::PistolWalk, "Kevin_Pistol_Walk" },
        { AnimationState::PistolShot, "Kevin_Pistol_Shot" },
        { AnimationState::CrouchIdle, "Kevin_Crouch_Idle" },
        { AnimationState::CrouchWalk, "Kevin_Crouch_Walk" },
        { AnimationState::CrouchPistolIdle, "Kevin_Crouch_Pistol_Idle" },
        { AnimationState::CrouchPistolWalk, "Kevin_Crouch_Pistol_Walk" },
        { AnimationState::CrouchRifleIdle, "Kevin_Crouch_Rifle_Idle" },
        { AnimationState::CrouchRifleWalk, "Kevin_Crouch_Rifle_Walk" },
        { AnimationState::RiflePeekLeft, "Kevin_Rifle_Peek_Left" },
        // Пустой слот пропускается. Пока клипы не назначены, NPC просто стоит.
        { AnimationState::Search1, "" },
        { AnimationState::Search2, "" },
        { AnimationState::Search3, "" }
    };

    std::vector<DirectX::XMFLOAT4X4> finalBoneMatrices;

    float GetClipSpeed(const std::string& clip) const
    {
        for (const auto& setting : clipSpeeds)
            if (setting.clip == clip && std::isfinite(setting.speed))
                return std::clamp(setting.speed, 0.1f, 5.0f);
        return 1.0f;
    }
};
