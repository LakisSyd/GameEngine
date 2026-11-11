#include "EditorUI.h"
#include <string>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <unordered_set>
#include <vector>
#include "SceneManager.h"
#include "AssetsLoadSystem.h"
#include "Gameplay/Lighting/SpotLightComponent.h"
#include "CameraController.h"
#include "cameraclass.h"
#include "ImGuizmo.h"
#include "ObjectDebugWindow.h"
#include <backends/imgui_impl_win32.h>
#include <imgui-master/backends/imgui_impl_dx11.h>
#include "PhysicsSystem.h"
#include "SubmarineJointSystem.h"
#include "FloodingSystem.h"
#include "WeaponComponent.h"
#include "Gameplay/Combat/WeaponSystem.h"
#include "Animation/WeaponIKSystem.h"
#include "Animation/MeleeActionJson.h"
#include "JointConnectionSystem.h"
#include "SubmarineSystem.h"
#include "CharacterSpawnSystem.h"
#include "CharacterControllerSystem.h"
#include "Gameplay/Player/CharacterPhysicsTuningComponent.h"
#include "Gameplay/Interaction/TriggerZoneComponent.h"
#include "Gameplay/Interaction/TriggerActionSystem.h"
#include "Gameplay/Inventory/InventoryComponent.h"
#include "Gameplay/Inventory/InventorySystem.h"
#include "Gameplay/Inventory/WeaponPickupSystem.h"
#include "Gameplay/AI/CrewTaskCoordinator.h"
#include "../../NPCComponents.h"
#include "../../NpcCommandSystem.h"
#include "Gameplay/AI/NpcNavMeshSystem.h"
#include "Gameplay/AI/NpcSimulationStudio.h"
#include "Gameplay/AI/NpcCombatDebugCommandSystem.h"

inline btQuaternion XMFloat4ToBtQuaternion(const DirectX::XMFLOAT4& q)
{
    return btQuaternion(q.x, q.y, q.z, q.w);
}

std::string BuildCollisionModelPath(const std::string& modelPath)
{
    std::filesystem::path path(modelPath);

    return (
        path.parent_path() /
        (path.stem().string() + "_collision" + path.extension().string())
        ).string();
}


// -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (UE5-Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦) ---------------------------------------------------
static constexpr ImVec4 COL_HEADER = ImVec4(0.13f, 0.13f, 0.13f, 1.0f);
static constexpr ImVec4 COL_SECTION = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
static constexpr ImVec4 COL_ACCENT = ImVec4(0.06f, 0.53f, 0.98f, 1.0f);
static constexpr ImVec4 COL_ACCENT_HOV = ImVec4(0.20f, 0.65f, 1.00f, 1.0f);
static constexpr ImVec4 COL_GREEN = ImVec4(0.13f, 0.69f, 0.30f, 1.0f);
static constexpr ImVec4 COL_GREEN_HOV = ImVec4(0.18f, 0.85f, 0.40f, 1.0f);
static constexpr ImVec4 COL_ORANGE = ImVec4(0.95f, 0.58f, 0.16f, 1.0f);
static constexpr ImVec4 COL_RED = ImVec4(0.75f, 0.18f, 0.18f, 1.0f);
static constexpr ImVec4 COL_TEXT_DIM = ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
static constexpr ImVec4 COL_SELECTED = ImVec4(0.06f, 0.45f, 0.80f, 0.35f);
static constexpr ImVec4 COL_X = ImVec4(0.86f, 0.27f, 0.27f, 1.0f);
static constexpr ImVec4 COL_Y = ImVec4(0.27f, 0.72f, 0.27f, 1.0f);
static constexpr ImVec4 COL_Z = ImVec4(0.27f, 0.47f, 0.86f, 1.0f);

static bool ContainsCaseInsensitive(const char* text, const char* query)
{
    if (!query || query[0] == '\0')
        return true;
    if (!text)
        return false;

    std::string haystack(text);
    std::string needle(query);
    std::transform(haystack.begin(), haystack.end(), haystack.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(needle.begin(), needle.end(), needle.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return haystack.find(needle) != std::string::npos;
}

static float SnapEditorValue(float value, float step)
{
    if (step <= 0.00001f)
        return value;
    return std::round(value / step) * step;
}

static const char* WorldAreaTypeLabel(WorldAreaType type)
{
    switch (type)
    {
    case WorldAreaType::Grass: return "Grass";
    case WorldAreaType::NoGrass: return "No Grass";
    case WorldAreaType::Forest: return "Forest";
    case WorldAreaType::Spawn: return "Spawn";
    case WorldAreaType::Trigger: return "Trigger";
    default: return "World";
    }
}
static const char* PlayerLocomotionStateName(PlayerLocomotionState state)
{
    switch (state)
    {
    case PlayerLocomotionState::Idle: return "Idle";
    case PlayerLocomotionState::Walk: return "Walk";
    case PlayerLocomotionState::Run: return "Run";
    case PlayerLocomotionState::Exhausted: return "Exhausted";
    default: return "Unknown";
    }
}
static const char* AnimationStateName(AnimationState state)
{
    switch (state)
    {
    case AnimationState::Idle: return "Idle";
    case AnimationState::Walk: return "Walk";
    case AnimationState::Run: return "Run";
    case AnimationState::RifleIdle: return "Rifle Idle";
    case AnimationState::RifleGun: return "Rifle Gun";
    case AnimationState::RifleRun: return "Rifle Run (full body)";
    case AnimationState::RifleWalk: return "Rifle Walk";
    case AnimationState::RifleHipIdle: return "Rifle Hip Idle";
    case AnimationState::Punch: return "Punch";
    case AnimationState::PistolIdle: return "Pistol Idle";
    case AnimationState::PistolWalk: return "Pistol Walk";
    case AnimationState::PistolShot: return "Pistol Shot";
    case AnimationState::CrouchIdle: return "Crouch Unarmed Idle";
    case AnimationState::CrouchWalk: return "Crouch Unarmed Walk";
    case AnimationState::CrouchPistolIdle: return "Crouch Pistol Idle";
    case AnimationState::CrouchPistolWalk: return "Crouch Pistol Walk";
    case AnimationState::CrouchRifleIdle: return "Crouch Rifle Idle";
    case AnimationState::CrouchRifleWalk: return "Crouch Rifle Walk";
	case AnimationState::RiflePeekLeft: return "Rifle Peek Left";
    case AnimationState::Search1: return "Search 1";
    case AnimationState::Search2: return "Search 2";
    case AnimationState::Search3: return "Search 3";
    default: return "Unknown";
    }
}

static void DrawPunchSettings(PlayerComponent& player)
{
    ImGui::DragFloat("Punch damage", &player.punchDamage, 0.25f, 0.0f, 200.0f);
    ImGui::DragFloat("Punch reach", &player.punchReach, 0.01f, 0.1f, 3.0f);
    ImGui::DragFloat("Contact radius", &player.punchContactRadius,
        0.005f, 0.01f, 1.0f);
    ImGui::SliderFloat("Contact moment", &player.punchContactPhase, 0.05f, 0.95f);
    ImGui::SliderFloat("Aim correction", &player.punchIKStrength, 0.0f, 1.0f);
    ImGui::SliderFloat("FP punch aim correction", &player.punchFirstPersonIKStrength,
        0.0f, 1.0f);
    ImGui::DragFloat3("Fist contact offset", &player.punchFistOffset.x,
        0.005f, -0.3f, 0.3f, "%.3f");
    ImGui::TextDisabled("Offset is relative to the left hand bone.");
}

static std::string AnimationStudioPath(const std::string& name)
{
    return MeleeActionJson::ProfilePath(name);
}

static void DrawFirstPersonClipFraming(AnimationComponent& animation,
    const std::string& clip, int variant, const char* title)
{
    if (clip.empty()) return;
    ImGui::PushID(title);
    ImGui::SeparatorText(title);
    ImGui::Text("Clip: %s", clip.c_str());
    auto findOffset = [&]() -> FirstPersonClipOffset*
    {
        for (auto& offset : animation.firstPersonClipOffsets)
            if (offset.clip == clip && offset.variant == variant)
                return &offset;
        return nullptr;
    };
    FirstPersonClipOffset* offset = findOffset();
    DirectX::XMFLOAT3 position = offset ? offset->position : DirectX::XMFLOAT3{};
    DirectX::XMFLOAT3 rotation = offset ? offset->rotation : DirectX::XMFLOAT3{};
    const bool positionChanged = ImGui::DragFloat3("Position offset (m)",
        &position.x, 0.01f);
    const bool rotationChanged = ImGui::DragFloat3("Rotation offset (deg)",
        &rotation.x, 0.25f);
    if (positionChanged || rotationChanged)
    {
        if (!offset)
        {
            animation.firstPersonClipOffsets.push_back({ clip, variant });
            offset = &animation.firstPersonClipOffsets.back();
        }
        offset->position = position;
        offset->rotation = rotation;
    }
    if (offset && ImGui::Button("Reset this clip offset"))
    {
        auto& offsets = animation.firstPersonClipOffsets;
        offsets.erase(std::remove_if(offsets.begin(), offsets.end(),
            [&](const FirstPersonClipOffset& item)
            { return item.clip == clip && item.variant == variant; }), offsets.end());
    }
    ImGui::TextDisabled("Only this first-person clip uses this offset.");
    ImGui::PopID();
}

static void DrawClipSpeed(AnimationComponent& animation,
    const std::string& clip, const char* label)
{
    if (clip.empty()) return;
    float speed = animation.GetClipSpeed(clip);
    if (ImGui::DragFloat(label, &speed, 0.05f, 0.1f, 5.0f, "%.2fx"))
    {
        speed = std::clamp(speed, 0.1f, 5.0f);
        auto& settings = animation.clipSpeeds;
        auto it = std::find_if(settings.begin(), settings.end(),
            [&](const AnimationClipSpeed& item) { return item.clip == clip; });
        if (it != settings.end()) it->speed = speed;
        else settings.push_back({ clip, speed });
    }
}

static float LerpAnimationAngle(float from, float to, float alpha)
{
    float delta = std::fmod(to - from + 180.0f, 360.0f);
    if (delta < 0.0f) delta += 360.0f;
    delta -= 180.0f;
    return from + delta * alpha;
}

static std::string NormalizeAnimationNodeName(std::string name)
{
    if (const size_t marker = name.find("_scaleCompensation");
        marker != std::string::npos)
        name.resize(marker);
    const size_t separator = name.find_last_of('_');
    if (separator != std::string::npos && separator + 1 < name.size() &&
        std::all_of(name.begin() + separator + 1, name.end(),
            [](unsigned char c) { return std::isdigit(c) != 0; }))
        name.resize(separator);
    return name;
}

static bool AnimationNodeNamesMatch(const std::string& left,
    const std::string& right)
{
    return NormalizeAnimationNodeName(left) ==
        NormalizeAnimationNodeName(right);
}

static float AnimationClipPreviewEnd(float duration)
{
    if (!std::isfinite(duration) || duration <= 0.0f) return 0.0f;
    // Animation sampling wraps with fmod(duration). Keep the editor playhead a
    // small distance before the loop boundary so the end button shows the last
    // pose instead of frame zero and cannot create an unreachable key.
    const float epsilon = (std::min)(duration * 0.5f,
        (std::max)(0.0001f, duration * 0.00001f));
    return (std::max)(0.0f, duration - epsilon);
}

static bool EvaluatePoseCorrection(const AnimationComponent& animation,
    const std::string& clip, const std::string& bone, int variant, float time,
    DirectX::XMFLOAT3& position, DirectX::XMFLOAT3& rotation)
{
    bool hasVariantTrack = false;
    if (variant >= 0)
        for (const auto& key : animation.poseKeys)
            if (key.variant == variant && key.clip == clip &&
                AnimationNodeNamesMatch(key.bone, bone))
            {
                hasVariantTrack = true;
                break;
            }
    const int trackVariant = hasVariantTrack ? variant : -1;
    const AnimationPoseKey* before = nullptr;
    const AnimationPoseKey* after = nullptr;
    for (const auto& key : animation.poseKeys)
    {
        if (key.variant != trackVariant || key.clip != clip ||
            !AnimationNodeNamesMatch(key.bone, bone))
            continue;
        if (key.time <= time && (!before || key.time > before->time)) before = &key;
        if (key.time >= time && (!after || key.time < after->time)) after = &key;
    }
    if (!before) before = after;
    if (!after) after = before;
    if (!before || !after)
    {
        position = {};
        rotation = {};
        return false;
    }
    const float span = after->time - before->time;
    const float alpha = span > 0.00001f
        ? std::clamp((time - before->time) / span, 0.0f, 1.0f) : 0.0f;
    const auto lerp = [alpha](float a, float b) { return a + (b - a) * alpha; };
    position = {
        lerp(before->position.x, after->position.x),
        lerp(before->position.y, after->position.y),
        lerp(before->position.z, after->position.z) };
    rotation = {
        LerpAnimationAngle(before->rotation.x, after->rotation.x, alpha),
        LerpAnimationAngle(before->rotation.y, after->rotation.y, alpha),
        LerpAnimationAngle(before->rotation.z, after->rotation.z, alpha) };
    return true;
}

static AnimationPoseKey* FindPoseKey(AnimationComponent& animation,
    const std::string& clip, const std::string& bone, int variant, float time,
    float tolerance = 0.0005f)
{
    for (auto& key : animation.poseKeys)
        if (key.variant == variant && key.clip == clip &&
            AnimationNodeNamesMatch(key.bone, bone) &&
            std::abs(key.time - time) <= tolerance)
            return &key;
    return nullptr;
}

static AnimationPoseKey& EnsurePoseKey(AnimationComponent& animation,
    const std::string& clip, const std::string& bone, int variant, float time,
    const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& rotation)
{
    // A punch variant replaces the shared track once it owns any key. Clone the
    // shared curve first so creating the initial variant key cannot flatten the
    // rest of the animation.
    if (variant >= 0)
    {
        bool hasVariantTrack = false;
        for (const auto& key : animation.poseKeys)
            hasVariantTrack = hasVariantTrack ||
                (key.variant == variant && key.clip == clip &&
                    AnimationNodeNamesMatch(key.bone, bone));
        if (!hasVariantTrack)
        {
            const size_t originalCount = animation.poseKeys.size();
            for (size_t i = 0; i < originalCount; ++i)
                if (animation.poseKeys[i].variant == -1 &&
                    animation.poseKeys[i].clip == clip &&
                    AnimationNodeNamesMatch(animation.poseKeys[i].bone, bone))
                {
                    AnimationPoseKey clone = animation.poseKeys[i];
                    clone.variant = variant;
                    animation.poseKeys.push_back(std::move(clone));
                }
        }
    }
    if (AnimationPoseKey* existing = FindPoseKey(
            animation, clip, bone, variant, time))
        return *existing;
    AnimationPoseKey key;
    key.clip = clip;
    key.bone = bone;
    key.variant = variant;
    key.time = time;
    key.position = position;
    key.rotation = rotation;
    animation.poseKeys.push_back(std::move(key));
    return animation.poseKeys.back();
}

static constexpr const char* kLeftFistControl = "@CTRL_Fist.L";
static constexpr const char* kRightFistControl = "@CTRL_Fist.R";

static bool IsFistControl(const std::string& name)
{
    return name == kLeftFistControl || name == kRightFistControl;
}

static bool IsRightFistControl(const std::string& name)
{
    return name == kRightFistControl;
}

static float EvaluateFistControl(const AnimationComponent& animation,
    const std::string& clip, bool rightHand, int variant, float time)
{
    bool hasVariantTrack = false;
    if (variant >= 0)
        for (const auto& key : animation.fistKeys)
            if (key.variant == variant && key.clip == clip &&
                key.rightHand == rightHand)
            {
                hasVariantTrack = true;
                break;
            }
    const int activeVariant = hasVariantTrack ? variant : -1;
    const AnimationFistKey* before = nullptr;
    const AnimationFistKey* after = nullptr;
    for (const auto& key : animation.fistKeys)
    {
        if (key.variant != activeVariant || key.clip != clip ||
            key.rightHand != rightHand) continue;
        if (key.time <= time && (!before || key.time > before->time)) before = &key;
        if (key.time >= time && (!after || key.time < after->time)) after = &key;
    }
    if (!before) before = after;
    if (!after) after = before;
    if (!before || !after) return 0.0f;
    const float span = after->time - before->time;
    const float alpha = span > 0.00001f
        ? std::clamp((time - before->time) / span, 0.0f, 1.0f) : 0.0f;
    return std::clamp(before->amount + (after->amount - before->amount) * alpha,
        -1.0f, 1.0f);
}

static AnimationFistKey* FindFistKey(AnimationComponent& animation,
    const std::string& clip, bool rightHand, int variant, float time,
    float tolerance = 0.0005f)
{
    for (auto& key : animation.fistKeys)
        if (key.variant == variant && key.clip == clip &&
            key.rightHand == rightHand &&
            std::abs(key.time - time) <= tolerance)
            return &key;
    return nullptr;
}

static AnimationFistKey& EnsureFistKey(AnimationComponent& animation,
    const std::string& clip, bool rightHand, int variant, float time,
    float amount)
{
    if (AnimationFistKey* existing = FindFistKey(animation, clip,
            rightHand, variant, time))
        return *existing;
    AnimationFistKey key;
    key.clip = clip;
    key.variant = variant;
    key.time = time;
    key.rightHand = rightHand;
    key.amount = std::clamp(amount, -1.0f, 1.0f);
    animation.fistKeys.push_back(std::move(key));
    return animation.fistKeys.back();
}

static std::string AnimationControlLabel(const std::string& name)
{
    if (name == kLeftFistControl) return "Fist Left";
    if (name == kRightFistControl) return "Fist Right";
    if (name == kAnimationLeftHandIKControl) return "Left Hand IK Target (move)";
    if (name == kAnimationRightHandIKControl) return "Right Hand IK Target (move)";
    if (name == kAnimationLeftElbowPoleControl) return "Left Elbow Pole (move)";
    if (name == kAnimationRightElbowPoleControl) return "Right Elbow Pole (move)";
    if (name == kAnimationHeadLookIKControl) return "Head Look IK Target (move)";
    const size_t separator = name.find(':');
    std::string label = separator == std::string::npos
        ? name : name.substr(separator + 1);
    const std::pair<const char*, const char*> words[] = {
        { "Left", "Left " }, { "Right", "Right " },
        { "ForeArm", "Forearm" }, { "UpLeg", "Upper Leg" } };
    for (const auto& [from, to] : words)
        if (label.rfind(from, 0) == 0)
        {
            label.replace(0, std::strlen(from), to);
            break;
        }
    if (label == "Left Hand") return "Left Hand (rotation only)";
    if (label == "Right Hand") return "Right Hand (rotation only)";
    return label;
}

static int AnimationControlGroup(const std::string& name)
{
    if (IsFistControl(name)) return 3;
    if (IsAnimationArmIKControl(name)) return 1;
    if (IsAnimationHeadIKControl(name)) return 0;
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower.find("finger") != std::string::npos ||
        lower.find("thumb") != std::string::npos ||
        lower.find("index") != std::string::npos ||
        lower.find("middle") != std::string::npos ||
        lower.find("ring") != std::string::npos ||
        lower.find("pinky") != std::string::npos) return 3;
    if (lower.find("arm") != std::string::npos ||
        lower.find("hand") != std::string::npos ||
        lower.find("shoulder") != std::string::npos) return 1;
    if (lower.find("leg") != std::string::npos ||
        lower.find("foot") != std::string::npos ||
        lower.find("toe") != std::string::npos) return 2;
    return 0;
}

static bool AnimationControlCanTranslate(const std::string& name)
{
    if (IsAnimationIKControl(name)) return true;
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    // Root motion is a real translation. Limb joints should be rotated or
    // driven by an IK target; translating them changes the bind-chain length.
    return lower.find("hips") != std::string::npos ||
        lower.find("root") != std::string::npos;
}

static bool AnimationControlCanRotate(const std::string& name)
{
    return !IsAnimationElbowPoleControl(name) &&
        !IsAnimationHeadIKControl(name);
}

static bool SaveAnimationStudio(const std::string& path,
    const AnimationComponent& animation, const AnimationComponent* firstPerson,
    const PlayerComponent* player, bool worldOnly = false)
{
    if (path.empty()) return false;
    try
    {
        json data = json::object();
        // Merge into an existing preset. If the FP arms are temporarily absent,
        // a third-person save must not erase previously authored per-clip FP keys.
        if (std::filesystem::exists(path))
        {
            std::ifstream existing(path);
            if (!existing) return false;
            existing >> data;
            if (!data.is_object()) return false;
        }
        data["version"] = 2;
        data["animation_bindings"] = json::array();
        for (const auto& binding : animation.animations)
            data["animation_bindings"].push_back({
                { "state", static_cast<int>(binding.state) }, { "name", binding.name } });
        MeleeActionJson::Write(data, animation, "punch_variants");
        if (worldOnly)
        {
            data["view"] = "third_person";
            if (auto actions = data.find("actions");
                actions != data.end() && actions->is_array())
                for (auto& action : *actions)
                {
                    action.erase("first_person_clip");
                    action.erase("move_first_person_clip");
                }
            if (auto variants = data.find("punch_variants");
                variants != data.end() && variants->is_array())
                for (auto& variant : *variants)
                    variant.erase("first_person_clip");
            data.erase("first_person_clip_offsets");
            data.erase("first_person_punch_clip");
            data.erase("first_person_pose_keys");
            data.erase("first_person_fist_keys");
        }
        data["pose_keys"] = json::array();
        for (const auto& key : animation.poseKeys)
            data["pose_keys"].push_back({
                { "clip", key.clip }, { "bone", key.bone }, { "variant", key.variant },
                { "time", key.time },
                { "position", { key.position.x, key.position.y, key.position.z } },
                { "rotation", { key.rotation.x, key.rotation.y, key.rotation.z } } });
        data["fist_keys"] = json::array();
        for (const auto& key : animation.fistKeys)
            data["fist_keys"].push_back({
                { "clip", key.clip }, { "variant", key.variant },
                { "time", key.time }, { "right_hand", key.rightHand },
                { "amount", key.amount } });
        if (firstPerson && !worldOnly)
        {
            for (const auto& binding : firstPerson->animations)
                if (binding.state == AnimationState::Punch)
                    data["first_person_punch_clip"] = binding.name;
            data["first_person_pose_keys"] = json::array();
            for (const auto& key : firstPerson->poseKeys)
                data["first_person_pose_keys"].push_back({
                    { "clip", key.clip }, { "bone", key.bone },
                    { "variant", key.variant }, { "time", key.time },
                    { "position", { key.position.x, key.position.y, key.position.z } },
                    { "rotation", { key.rotation.x, key.rotation.y, key.rotation.z } } });
            data["first_person_fist_keys"] = json::array();
            for (const auto& key : firstPerson->fistKeys)
                data["first_person_fist_keys"].push_back({
                    { "clip", key.clip }, { "variant", key.variant },
                    { "time", key.time }, { "right_hand", key.rightHand },
                    { "amount", key.amount } });
        }
        if (player)
        {
            // Имена совпадают с полями сцены; старые сцены и профили оружия не меняются.
            data["player_punch_damage"] = player->punchDamage;
            data["player_punch_reach"] = player->punchReach;
            data["player_punch_contact_radius"] = player->punchContactRadius;
            data["player_punch_contact_phase"] = player->punchContactPhase;
            data["player_punch_ik_strength"] = player->punchIKStrength;
            data["player_punch_fp_ik_strength"] = player->punchFirstPersonIKStrength;
            data["player_punch_fist_offset"] = json::array({
                player->punchFistOffset.x, player->punchFistOffset.y,
                player->punchFistOffset.z });
        }
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        std::ofstream output(path);
        if (!output) return false;
        output << data.dump(2);
        return output.good();
    }
    catch (const std::exception&) { return false; }
}

static bool LoadAnimationStudio(const std::string& path,
    AnimationComponent& animation, AnimationComponent* firstPerson,
    PlayerComponent* player)
{
    if (path.empty()) return false;
    try
    {
        std::ifstream input(path);
        if (!input) return false;
        json data;
        input >> data;
        if (!data.is_object()) return false;
        if (auto it = data.find("animation_bindings");
            it != data.end() && it->is_array())
        {
            for (const auto& saved : *it)
            {
                if (!saved.is_object() || !saved.contains("state") ||
                    !saved.contains("name") || !saved["state"].is_number_integer() ||
                    !saved["name"].is_string()) continue;
                const int state = saved["state"].get<int>();
                for (auto& binding : animation.animations)
                    if (static_cast<int>(binding.state) == state)
                    {
                        binding.name = saved["name"].get<std::string>();
                        binding.index = -1;
                    }
            }
        }
        MeleeActionJson::Read(data, animation, "punch_variants");
        MeleeActionJson::ReadPoseKeys(data, animation, "pose_keys");
        MeleeActionJson::ReadFistKeys(data, animation, "fist_keys");
        if (firstPerson)
        {
            if (auto it = data.find("first_person_punch_clip");
                it != data.end() && it->is_string())
                for (auto& binding : firstPerson->animations)
                    if (binding.state == AnimationState::Punch)
                    {
                        binding.name = it->get<std::string>();
                        binding.index = -1;
                    }
            MeleeActionJson::ReadPoseKeys(data, *firstPerson,
                "first_person_pose_keys");
            MeleeActionJson::ReadFistKeys(data, *firstPerson,
                "first_person_fist_keys");
        }
        if (player)
        {
            player->punchDamage = data.value("player_punch_damage", player->punchDamage);
            player->punchReach = data.value("player_punch_reach", player->punchReach);
            player->punchContactRadius = data.value("player_punch_contact_radius", player->punchContactRadius);
            player->punchContactPhase = data.value("player_punch_contact_phase", player->punchContactPhase);
            player->punchIKStrength = data.value("player_punch_ik_strength", player->punchIKStrength);
            player->punchFirstPersonIKStrength = data.value(
                "player_punch_fp_ik_strength", player->punchFirstPersonIKStrength);
            if (auto it = data.find("player_punch_fist_offset");
                it != data.end() && it->is_array() && it->size() == 3)
                player->punchFistOffset = { (*it)[0].get<float>(),
                    (*it)[1].get<float>(), (*it)[2].get<float>() };
            player->punchDamage = (std::max)(0.0f, player->punchDamage);
            player->punchReach = std::clamp(player->punchReach, 0.1f, 3.0f);
            player->punchContactRadius = std::clamp(player->punchContactRadius, 0.01f, 1.0f);
            player->punchContactPhase = std::clamp(player->punchContactPhase, 0.05f, 0.95f);
            player->punchIKStrength = std::clamp(player->punchIKStrength, 0.0f, 1.0f);
            player->punchFirstPersonIKStrength = std::clamp(
                player->punchFirstPersonIKStrength, 0.0f, 1.0f);
        }
        return true;
    }
    catch (const std::exception&) { return false; }
}

static bool InputStringValue(const char* label, std::string& value)
{
    char buffer[512]{};
    strncpy_s(buffer, value.c_str(), _TRUNCATE);
    if (!ImGui::InputText(label, buffer, sizeof(buffer)))
        return false;
    value = buffer;
    return true;
}

static void SetAnimationBindingName(
    AnimationComponent* animation, AnimationState state, const std::string& name)
{
    if (!animation) return;
    for (AnimationBinding& binding : animation->animations)
    {
        if (binding.state != state) continue;
        binding.name = name;
        binding.index = -1;
        return;
    }
}

static void DrawWeaponProfileControls(
    SceneManager& sceneManager, WeaponComponent& weapon,
    AnimationComponent* ownerAnimation, std::string& status,
    bool& visualReloadRequested)
{
    namespace fs = std::filesystem;
    ImGui::SeparatorText("Per-weapon preset");
    InputStringValue("Preset name##weapon_profile", weapon.gripProfileName);
    ImGui::SameLine();
    if (ImGui::Button("Name from model##weapon_profile"))
    {
        const std::string stem = fs::path(weapon.visualModelPath).stem().string();
        weapon.gripProfileName.clear();
        for (unsigned char c : stem)
        {
            if (weapon.gripProfileName.size() == 64) break;
            weapon.gripProfileName += (std::isalnum(c) || c == '_' || c == '-')
                ? static_cast<char>(c) : '_';
        }
    }
    const std::string profilePath =
        SceneManager::WeaponProfilePath(weapon.gripProfileName);
    if (profilePath.empty())
        ImGui::TextDisabled("Enter a name using letters, digits, _ or - (max 64).");
    else
    {
        ImGui::TextDisabled("Third person: %s", profilePath.c_str());
        ImGui::TextDisabled("First person: %s",
            fs::path(profilePath).replace_extension(".fp.json").string().c_str());
    }

    auto applyProfile = [&]()
    {
        if (!sceneManager.LoadWeaponProfile(weapon))
        {
            status = "Preset file was not found or could not be read.";
            return;
        }
        SetAnimationBindingName(ownerAnimation, AnimationState::RifleIdle,
            weapon.rifleIdleAnimation);
        SetAnimationBindingName(ownerAnimation, AnimationState::RifleGun,
            weapon.rifleGunAnimation);
        visualReloadRequested = true;
        status = "Loaded weapon preset: " + weapon.gripProfileName;
    };

    std::vector<std::string> savedPresets;
    const fs::path directory("assets/config/weapons");
    std::error_code error;
    if (fs::exists(directory, error))
        for (fs::directory_iterator it(directory, error), end;
            !error && it != end; it.increment(error))
            if (it->is_regular_file() && it->path().extension() == ".json")
            {
                const std::string stem = it->path().stem().string();
                if (stem.size() < 3 || stem.substr(stem.size() - 3) != ".fp")
                    savedPresets.push_back(stem);
            }
    std::sort(savedPresets.begin(), savedPresets.end());
    if (ImGui::BeginCombo("Saved presets##weapon_profile",
        weapon.gripProfileName.empty() ? "Choose preset" : weapon.gripProfileName.c_str()))
    {
        for (const std::string& name : savedPresets)
            if (ImGui::Selectable(name.c_str(), name == weapon.gripProfileName))
            {
                weapon.gripProfileName = name;
                applyProfile();
            }
        ImGui::EndCombo();
    }
    ImGui::BeginDisabled(profilePath.empty());
    if (ImGui::Button("Save current as preset##weapon_profile"))
        status = sceneManager.SaveWeaponProfile(weapon)
            ? "Saved third-person and first-person setup: " + weapon.gripProfileName
            : "Could not save weapon preset.";
    ImGui::SameLine();
    if (ImGui::Button("Reload preset##weapon_profile"))
        applyProfile();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Use scene values##weapon_profile"))
    {
        weapon.gripProfileName.clear();
        status = "Preset unlinked; save the scene to keep these values.";
    }
    if (!status.empty()) ImGui::TextWrapped("%s", status.c_str());
}

static bool SetLeftWristTargetFromMatrix(
    WeaponComponent& weapon,
    const DirectX::XMMATRIX& wristInWeapon,
    bool firstPerson);

static bool CaptureLeftWristTarget(
    WeaponComponent& weapon, const AssimpModelClass* model,
    const AnimationComponent* animation, bool firstPerson = false)
{
    if (!model || !animation) return false;
    using namespace DirectX;
    XMMATRIX rightHand, leftHand;
    if (!model->TryGetBoneModelTransform(
            *animation, weapon.rightHandBone, rightHand) ||
        !model->TryGetBoneModelTransform(
            *animation, weapon.leftHandBone, leftHand))
        return false;

    const XMMATRIX weaponModel =
        WeaponIKSystem::GripMatrix(firstPerson
            ? weapon.firstPersonGrip : weapon.worldGrip) * rightHand;
    const XMMATRIX wristInWeapon =
        leftHand * XMMatrixInverse(nullptr, weaponModel);
    return SetLeftWristTargetFromMatrix(
        weapon, wristInWeapon, firstPerson);
}

static bool SetLeftWristTargetFromMatrix(
    WeaponComponent& weapon,
    const DirectX::XMMATRIX& wristInWeapon,
    bool firstPerson)
{
    using namespace DirectX;
    XMVECTOR scale, rotation, translation;
    if (!XMMatrixDecompose(&scale, &rotation, &translation, wristInWeapon))
        return false;
    XMFLOAT3& targetPosition = firstPerson
        ? weapon.firstPersonLeftHandGrip : weapon.leftHandGrip;
    XMFLOAT3& targetRotation = firstPerson
        ? weapon.firstPersonLeftHandRotation : weapon.leftHandRotation;
    XMStoreFloat3(&targetPosition, translation);
    XMFLOAT4 q{};
    XMStoreFloat4(&q, rotation);
    // DirectX собирает поворот в порядке yaw * pitch * roll, поэтому здесь
    // используется соответствующее извлечение углов Эйлера.
    const float sinPitch = std::clamp(
        2.0f * (q.w * q.x - q.y * q.z), -1.0f, 1.0f);
    const float pitch = std::asin(sinPitch);
    const float yaw = std::atan2(
        2.0f * (q.x * q.z + q.w * q.y),
        1.0f - 2.0f * (q.x * q.x + q.y * q.y));
    const float roll = std::atan2(
        2.0f * (q.x * q.y + q.w * q.z),
        1.0f - 2.0f * (q.x * q.x + q.z * q.z));
    const auto wrap = [](float angle) {
        while (angle > XM_PI) angle -= XM_2PI;
        while (angle < -XM_PI) angle += XM_2PI;
        return angle;
    };
    const XMFLOAT3 first{ pitch, yaw, roll };
    const XMFLOAT3 second{ XM_PI - pitch,
        wrap(yaw - XM_PI), wrap(roll - XM_PI) };
    const float firstSize = std::fabs(first.x) + std::fabs(first.y) + std::fabs(first.z);
    const float secondSize = std::fabs(second.x) + std::fabs(second.y) + std::fabs(second.z);
    targetRotation = secondSize < firstSize ? second : first;
    return true;
}

static void SetGripPivotPreservingPose(
    WeaponGripTransform& grip, const DirectX::XMFLOAT3& pivot)
{
    const auto before = WeaponIKSystem::AttachmentPosition(grip);
    grip.pivot = pivot;
    const auto after = WeaponIKSystem::AttachmentPosition(grip);
    grip.position.x += before.x - after.x;
    grip.position.y += before.y - after.y;
    grip.position.z += before.z - after.z;
}

static void RotateGripInWeaponSpace(
    WeaponGripTransform& grip, int axis, float degrees)
{
    using namespace DirectX;
    const XMVECTOR direction = axis == 0 ? XMVectorSet(1, 0, 0, 0)
        : axis == 1 ? XMVectorSet(0, 1, 0, 0)
        : XMVectorSet(0, 0, 1, 0);
    const XMMATRIX rotated = XMMatrixRotationAxis(
        direction, XMConvertToRadians(degrees)) *
        XMMatrixRotationRollPitchYaw(
            grip.rotation.x, grip.rotation.y, grip.rotation.z);
    XMVECTOR scale, quaternion, translation;
    if (!XMMatrixDecompose(&scale, &quaternion, &translation, rotated))
        return;
    XMFLOAT4 q{};
    XMStoreFloat4(&q, quaternion);
    grip.rotation = {
        std::asin(std::clamp(2.0f * (q.w * q.x - q.y * q.z), -1.0f, 1.0f)),
        std::atan2(2.0f * (q.x * q.z + q.w * q.y),
            1.0f - 2.0f * (q.x * q.x + q.y * q.y)),
        std::atan2(2.0f * (q.x * q.y + q.w * q.z),
            1.0f - 2.0f * (q.x * q.x + q.z * q.z))
    };
}

static void DrawWeaponBallisticsEditor(WeaponComponent& weapon)
{
    ImGui::SeparatorText("Ballistics / muzzle flash (saved with weapon)");
    ImGui::DragInt("Magazine size", &weapon.magazineSize, 1.0f, 1, 1000);
    weapon.magazineSize = (std::max)(1, weapon.magazineSize);
    ImGui::DragInt("Loaded rounds", &weapon.ammoInMagazine, 1.0f, 0, weapon.magazineSize);
    weapon.ammoInMagazine = std::clamp(
        weapon.ammoInMagazine, 0, weapon.magazineSize);
    ImGui::DragInt("Reserve ammunition", &weapon.reserveAmmo, 1.0f, 0, 100000);
    weapon.reserveAmmo = (std::max)(0, weapon.reserveAmmo);
    ImGui::DragFloat("Reload duration", &weapon.reloadDuration,
        0.05f, 0.05f, 60.0f, "%.2f s");
    weapon.reloadDuration = std::clamp(weapon.reloadDuration, 0.05f, 60.0f);
    ImGui::Text("Runtime: %d / %d | reserve %d | %s %.2f s",
        weapon.ammoInMagazine, weapon.magazineSize, weapon.reserveAmmo,
        weapon.isReloading ? "reloading" : "ready", weapon.reloadTimer);
    ImGui::DragFloat("Spread half-angle (deg)", &weapon.spreadDegrees, 0.05f, 0.0f, 20.0f);
    ImGui::DragFloat("Moving spread / recoil multiplier", &weapon.movingSpreadMultiplier, 0.05f, 1.0f, 10.0f);
    ImGui::DragFloat("Recoil per shot (deg)", &weapon.recoilPerShotDegrees, 0.05f, 0.0f, 10.0f);
    ImGui::DragFloat("Maximum recoil (deg)", &weapon.maxRecoilDegrees, 0.05f, 0.0f, 20.0f);
    ImGui::DragFloat("Recoil recovery (deg/s)", &weapon.recoilRecoveryDegrees, 0.05f, 0.0f, 30.0f);
    ImGui::TextDisabled("Standing = stable; moving = wider cone. Sustained fire builds recoil.");
    ImGui::Checkbox("Muzzle flash", &weapon.muzzleFlashEnabled);
    ImGui::DragFloat("Muzzle flash scale", &weapon.muzzleFlashScale, 0.02f, 0.1f, 4.0f);
}

static void DrawWeaponGripEditor(
    WeaponComponent& weapon, AnimationComponent* ownerAnimation = nullptr,
    const AssimpModelClass* ownerModel = nullptr,
    bool* visualReloadRequested = nullptr)
{
    if (!ImGui::CollapsingHeader(
        "Weapon Grip Editor##weapon_grip",
        ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    DrawWeaponBallisticsEditor(weapon);
    ImGui::SeparatorText("Visual asset and animation");
    if (InputStringValue("Model Path##weapon_grip", weapon.visualModelPath) &&
        visualReloadRequested)
        *visualReloadRequested = true;
    if (ImGui::BeginCombo("Choose GLB##weapon_grip", "Browse assets/model"))
    {
        namespace fs = std::filesystem;
        std::vector<fs::path> models;
        std::error_code error;
        const fs::path directory("assets/model");
        if (fs::exists(directory, error))
            for (fs::directory_iterator it(directory, error), end;
                !error && it != end; it.increment(error))
                if (it->is_regular_file() && it->path().extension() == ".glb")
                    models.push_back(it->path());
        std::sort(models.begin(), models.end());
        for (const auto& path : models)
        {
            const std::string label = path.filename().string();
            if (ImGui::Selectable(label.c_str(),
                fs::path(weapon.visualModelPath) == path))
            {
                weapon.visualModelPath = path.generic_string();
                if (visualReloadRequested) *visualReloadRequested = true;
            }
        }
        ImGui::EndCombo();
    }
    InputStringValue("Right Hand Bone##weapon_grip", weapon.rightHandBone);
    ImGui::Checkbox("Pistol animation stance##weapon_grip", &weapon.pistolPose);
    if (InputStringValue("Rifle Idle Clip##weapon_grip", weapon.rifleIdleAnimation))
        SetAnimationBindingName(ownerAnimation, AnimationState::RifleIdle,
            weapon.rifleIdleAnimation);
    if (InputStringValue("Rifle Gun Clip##weapon_grip", weapon.rifleGunAnimation))
        SetAnimationBindingName(ownerAnimation, AnimationState::RifleGun,
            weapon.rifleGunAnimation);
    ImGui::Checkbox("World Visual##weapon_grip", &weapon.showWorldVisual);
    ImGui::SameLine();
    ImGui::Checkbox("First Person Visual##weapon_grip", &weapon.showFirstPersonVisual);

    ImGui::SeparatorText("Camera aim");
    ImGui::DragFloat(
        "Arms Pitch Scale##weapon_grip",
        &weapon.firstPersonPitchScale,
        0.01f,
        -2.0f,
        2.0f,
        "%.2f");
    float pitchOffsetDegrees =
        DirectX::XMConvertToDegrees(weapon.firstPersonPitchOffset);
    if (ImGui::DragFloat(
        "Arms Pitch Offset##weapon_grip",
        &pitchOffsetDegrees,
        0.25f,
        -90.0f,
        90.0f,
        "%.1f deg"))
    {
        weapon.firstPersonPitchOffset =
            DirectX::XMConvertToRadians(pitchOffsetDegrees);
    }

    ImGui::SeparatorText("Grip transforms");
    if (std::filesystem::path(weapon.visualModelPath).filename() ==
        "Rifle_M4_CC0.glb")
    {
        if (ImGui::Button("Fit M4 to Kevin hands##weapon_grip"))
        {
            weapon.worldGrip.position = { 0.0298783f, 0.0213676f, 0.1143637f };
            weapon.worldGrip.rotation = { -0.0080474f, 0.0122972f, -1.870682f };
            weapon.worldGrip.scale = { 0.8f, 0.8f, 0.8f };
            weapon.leftHandGrip = { -0.365106f, 0.0318003f, -0.306782f };
            weapon.leftHandRotation = { 2.0677649f, -0.4732285f, -0.1169028f };
            weapon.enableLeftHandIK = true;
        }
        ImGui::TextDisabled("Third-person M4 grip and left wrist target for Kevin.");
    }
    if (ImGui::BeginTable(
        "WeaponGripTransformGrid",
        3,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_NoSavedSettings |
        ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 95.0f);
        ImGui::TableSetupColumn("World", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("First Person", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Position");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat3("##weapon_world_position", &weapon.worldGrip.position.x, 0.005f);
        ImGui::TableSetColumnIndex(2);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat3("##weapon_fp_position", &weapon.firstPersonGrip.position.x, 0.005f);

        DirectX::XMFLOAT3 worldDegrees = {
            DirectX::XMConvertToDegrees(weapon.worldGrip.rotation.x),
            DirectX::XMConvertToDegrees(weapon.worldGrip.rotation.y),
            DirectX::XMConvertToDegrees(weapon.worldGrip.rotation.z) };
        DirectX::XMFLOAT3 fpDegrees = {
            DirectX::XMConvertToDegrees(weapon.firstPersonGrip.rotation.x),
            DirectX::XMConvertToDegrees(weapon.firstPersonGrip.rotation.y),
            DirectX::XMConvertToDegrees(weapon.firstPersonGrip.rotation.z) };

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Rotation");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::DragFloat3("##weapon_world_rotation", &worldDegrees.x, 0.5f))
        {
            weapon.worldGrip.rotation = {
                DirectX::XMConvertToRadians(worldDegrees.x),
                DirectX::XMConvertToRadians(worldDegrees.y),
                DirectX::XMConvertToRadians(worldDegrees.z) };
        }
        ImGui::TableSetColumnIndex(2);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::DragFloat3("##weapon_fp_rotation", &fpDegrees.x, 0.5f))
        {
            weapon.firstPersonGrip.rotation = {
                DirectX::XMConvertToRadians(fpDegrees.x),
                DirectX::XMConvertToRadians(fpDegrees.y),
                DirectX::XMConvertToRadians(fpDegrees.z) };
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Scale");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat3(
            "##weapon_world_scale", &weapon.worldGrip.scale.x,
            0.005f, 0.01f, 10.0f);
        ImGui::TableSetColumnIndex(2);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat3(
            "##weapon_fp_scale", &weapon.firstPersonGrip.scale.x,
            0.005f, 0.01f, 10.0f);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Rotation center");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
        auto worldPivot = weapon.worldGrip.pivot;
        if (ImGui::DragFloat3("##weapon_world_pivot", &worldPivot.x, 0.005f))
            SetGripPivotPreservingPose(weapon.worldGrip, worldPivot);
        ImGui::TableSetColumnIndex(2);
        ImGui::SetNextItemWidth(-1.0f);
        auto fpPivot = weapon.firstPersonGrip.pivot;
        if (ImGui::DragFloat3("##weapon_fp_pivot", &fpPivot.x, 0.005f))
            SetGripPivotPreservingPose(weapon.firstPersonGrip, fpPivot);

        ImGui::EndTable();
    }

    float uniformWorldScale = weapon.worldGrip.scale.x;
    if (ImGui::DragFloat("Uniform World Scale##weapon_grip",
        &uniformWorldScale, 0.005f, 0.01f, 10.0f))
        weapon.worldGrip.scale = {
            uniformWorldScale, uniformWorldScale, uniformWorldScale };

    if (ImGui::Button("World -> FP##weapon_grip", ImVec2(-1.0f, 0.0f)))
        weapon.firstPersonGrip = weapon.worldGrip;
    if (ImGui::Button("FP -> World##weapon_grip", ImVec2(-1.0f, 0.0f)))
        weapon.worldGrip = weapon.firstPersonGrip;

    ImGui::TextDisabled("Quick barrel-axis correction");
    const float quarterTurn = DirectX::XM_PIDIV2;
    if (ImGui::Button("X -90##weapon_grip"))
        weapon.worldGrip.rotation.x -= quarterTurn;
    ImGui::SameLine();
    if (ImGui::Button("X +90##weapon_grip"))
        weapon.worldGrip.rotation.x += quarterTurn;
    ImGui::SameLine();
    if (ImGui::Button("Y -90##weapon_grip"))
        weapon.worldGrip.rotation.y -= quarterTurn;
    ImGui::SameLine();
    if (ImGui::Button("Y +90##weapon_grip"))
        weapon.worldGrip.rotation.y += quarterTurn;

    if (ImGui::Button("Copy World Rotation To FP##weapon_grip", ImVec2(-1.0f, 0.0f)))
        weapon.firstPersonGrip.rotation = weapon.worldGrip.rotation;

    if (ImGui::Button("Reset Grip Preset##weapon_grip", ImVec2(-1.0f, 0.0f)))
    {
        weapon.worldGrip = WeaponGripTransform{};
        weapon.firstPersonGrip = WeaponGripTransform{};
    }

    ImGui::SeparatorText("Sockets");
    ImGui::DragFloat3(
        "Muzzle Position##weapon_grip",
        &weapon.muzzlePosition.x,
        0.005f);
    ImGui::Checkbox("Enable Third-person Left Hand IK##weapon_grip", &weapon.enableLeftHandIK);
    if (ImGui::Button("Capture left wrist from current pose##weapon_grip"))
        CaptureLeftWristTarget(weapon, ownerModel, ownerAnimation);
    if (ownerAnimation)
        ImGui::Text("Left IK: %s | wrist error: %.3f m",
            ownerAnimation->leftHandIKApplied ? "active" : "inactive",
            ownerAnimation->leftHandIKError);
    ImGui::BeginDisabled(!weapon.enableLeftHandIK);
    ImGui::DragFloat3(
        "Left Hand Grip##weapon_grip",
        &weapon.leftHandGrip.x,
        0.005f);
    ImGui::DragFloat3("Left Hand Rotation (rad)##weapon_grip", &weapon.leftHandRotation.x, 0.01f);
    InputStringValue("Left Upper Arm Bone##weapon_grip", weapon.leftUpperArmBone);
    InputStringValue("Left Forearm Bone##weapon_grip", weapon.leftForearmBone);
    InputStringValue("Left Hand Bone##weapon_grip", weapon.leftHandBone);
    ImGui::EndDisabled();
    ImGui::TextDisabled("World wrist target is in weapon space; FP target is in Socket Studio.");
    ImGui::TextDisabled(
        "Grip updates live. A new model loads after Reload Visual or on next Play.");
}

template <typename RegistryT>
static std::string EditorEntityDebugLabel(RegistryT& registry, Entity entity)
{
    if (entity == INVALID_ENTITY)
        return "None";

    if (auto* room = registry.GetComponent<RoomComponent>(entity))
        return room->roomName + " [" + std::to_string(entity) + "]";

    if (auto* name = registry.GetComponent<NameComponent>(entity))
        return name->name + " [" + std::to_string(entity) + "]";

    return "Entity " + std::to_string(entity);
}

// Inventory authoring uses the same two mutually exclusive states as runtime:
// a stored weapon is owned and hidden, while a dropped weapon is active,
// interactable and (when requested by WeaponPickupComponent) physical.
static void SetEditorStoredWeaponState(ECSRegistry& registry, Entity weapon)
{
    if (auto* pickup = registry.GetComponent<WeaponPickupComponent>(weapon))
        pickup->enabled = false;

    registry.RemoveComponent<InteractionComponent>(weapon);
    registry.RemoveComponent<RigidBodyComponent>(weapon);
    registry.RemoveComponent<ActiveComponent>(weapon);
}

static void RestoreEditorWorldPickup(
    ECSRegistry& registry,
    Entity inventoryOwner,
    Entity weapon,
    std::size_t sourceSlot)
{
    auto* pickup = registry.GetComponent<WeaponPickupComponent>(weapon);
    if (!pickup)
    {
        registry.AddComponent<WeaponPickupComponent>(
            weapon, WeaponPickupComponent{});
        pickup = registry.GetComponent<WeaponPickupComponent>(weapon);
    }

    auto* transform = registry.GetComponent<TransformComponent>(weapon);
    if (!transform)
    {
        registry.AddComponent<TransformComponent>(weapon, TransformComponent{});
        transform = registry.GetComponent<TransformComponent>(weapon);
    }
    if (transform)
    {
        if (const auto* ownerTransform =
            registry.GetComponent<TransformComponent>(inventoryOwner))
        {
            // Editor mode has no gameplay camera/forward vector. Place every
            // dropped slot beside its owner deterministically instead of
            // reviving the rifle at its stale pre-pickup location.
            transform->position = ownerTransform->position;
            transform->position.x +=
                0.75f + static_cast<float>(sourceSlot) * 0.35f;
            transform->position.y += 0.35f;
        }
    }

    if (pickup)
    {
        pickup->enabled = true;

        InteractionComponent interaction;
        interaction.enabled = true;
        interaction.maxDistance = 3.0f;
        interaction.prompt = pickup->prompt;
        registry.AddComponent<InteractionComponent>(weapon, std::move(interaction));

        if (pickup->createPhysicsOnDrop &&
            !registry.HasComponent<RigidBodyComponent>(weapon))
        {
            RigidBodyComponent body;
            body.mass = (std::max)(0.01f, pickup->dropMass);
            body.isDynamic = true;
            body.shapeType = RigidBodyComponent::Box;
            body.useManualSize = true;
            body.shapeSize = btVector3(
                (std::max)(0.05f, pickup->dropCollisionSize.x),
                (std::max)(0.05f, pickup->dropCollisionSize.y),
                (std::max)(0.05f, pickup->dropCollisionSize.z));
            registry.AddComponent<RigidBodyComponent>(weapon, std::move(body));

            // The editor normally owns an initialized Bullet world. Keeping the
            // authoring component is still enough for a later scene load when
            // a world is not available (for example, in a headless tool).
            if (PhysicsWorld::Get().GetWorld() && transform)
                PhysicsSystem::CreateRigidBody(registry, weapon);
        }
    }

    registry.AddComponent<ActiveComponent>(weapon, ActiveComponent{});
}

static InventoryOperation DropEditorInventorySlot(
    ECSRegistry& registry,
    Entity owner,
    std::size_t slot)
{
    const InventoryOperation result =
        InventorySystem::DropSlot(registry, owner, slot);
    if (result)
        RestoreEditorWorldPickup(registry, owner, result.item, result.slot);
    return result;
}

static InventoryOperation StoreEditorWeaponInSlot(
    ECSRegistry& registry,
    Entity owner,
    Entity weapon,
    std::size_t destinationSlot)
{
    InventoryComponent* inventory =
        InventorySystem::EnsureInventory(registry, owner);
    if (!inventory || !inventory->IsValidSlot(destinationSlot))
        return { InventoryStatus::InvalidSlot, weapon, destinationSlot };

    if (weapon == INVALID_ENTITY || !registry.IsAlive(weapon))
        return { InventoryStatus::InvalidItem, weapon, destinationSlot };
    if (!registry.HasComponent<WeaponComponent>(weapon))
        return { InventoryStatus::ItemIsNotWeapon, weapon, destinationSlot };

    const std::size_t existingSlot = inventory->FindItem(weapon);
    if (existingSlot != InventoryComponent::InvalidSlot)
    {
        if (existingSlot != destinationSlot)
        {
            const InventoryOperation swapped = InventorySystem::SwapSlots(
                registry, owner, existingSlot, destinationSlot);
            if (!swapped)
                return swapped;

            const Entity displaced = inventory->slots[existingSlot].item;
            SetEditorStoredWeaponState(registry, weapon);
            if (displaced != INVALID_ENTITY)
                SetEditorStoredWeaponState(registry, displaced);
        }
        else
        {
            SetEditorStoredWeaponState(registry, weapon);
        }

        return { InventoryStatus::Success, weapon, destinationSlot };
    }

    if (const auto* ownership =
        registry.GetComponent<WeaponOwnerComponent>(weapon))
    {
        if (ownership->owner != INVALID_ENTITY && ownership->owner != owner)
        {
            return {
                InventoryStatus::OwnedByAnotherEntity,
                weapon,
                destinationSlot
            };
        }
    }

    const bool destinationWasActive =
        inventory->activeSlot == destinationSlot;
    const Entity displaced = inventory->slots[destinationSlot].item;
    InventoryOperation dropped{
        InventoryStatus::EmptySlot,
        INVALID_ENTITY,
        destinationSlot
    };
    if (displaced != INVALID_ENTITY)
        dropped = DropEditorInventorySlot(registry, owner, destinationSlot);

    const InventoryOperation stored = InventorySystem::PickupWeapon(
        registry,
        owner,
        weapon,
        destinationSlot,
        destinationWasActive);

    if (!stored)
    {
        // Preserve the original slot if an unexpected validation failure
        // occurs after its previous item was restored to the world.
        if (dropped)
        {
            const InventoryOperation rollback = InventorySystem::PickupWeapon(
                registry,
                owner,
                dropped.item,
                destinationSlot,
                destinationWasActive);
            if (rollback)
                SetEditorStoredWeaponState(registry, dropped.item);
        }
        return stored;
    }

    SetEditorStoredWeaponState(registry, weapon);
    return stored;
}

static const char* EditorInventoryStatusMessage(InventoryStatus status)
{
    switch (status)
    {
    case InventoryStatus::Success: return "Inventory updated";
    case InventoryStatus::InvalidOwner: return "Inventory owner is invalid";
    case InventoryStatus::MissingInventory: return "Inventory component is missing";
    case InventoryStatus::InvalidItem: return "Selected weapon entity is invalid";
    case InventoryStatus::ItemIsNotWeapon: return "Selected entity is not a weapon";
    case InventoryStatus::OwnedByAnotherEntity: return "Weapon belongs to another entity";
    case InventoryStatus::InventoryFull: return "Destination slot is occupied";
    case InventoryStatus::AlreadyStored: return "Weapon is already stored";
    case InventoryStatus::InvalidSlot: return "Inventory slot is invalid";
    case InventoryStatus::EmptySlot: return "Inventory slot is empty";
    case InventoryStatus::ItemNotStored: return "Weapon is not stored in this inventory";
    default: return "Inventory action failed";
    }
}

// Shared inspector for every Bullet kinematic character. It deliberately does
// not know whether the entity is a Player or an NPC: a capsule colliding with a
// small ledge must use exactly the same rules in both cases.
static void RenderCharacterPhysicsTuning(ECSRegistry& registry, Entity entity)
{
    auto* controller = registry.GetComponent<CharacterController>(entity);
    if (!controller)
        return;

    auto* tuning = registry.GetComponent<CharacterPhysicsTuningComponent>(entity);
    if (!tuning)
    {
        registry.AddComponent<CharacterPhysicsTuningComponent>(
            entity, CharacterPhysicsTuningComponent{});
        tuning = registry.GetComponent<CharacterPhysicsTuningComponent>(entity);
    }
    if (!tuning)
        return;

    ImGui::PushID(static_cast<int>(entity));
    ImGui::TextWrapped(
        "These settings are shared by Player and NPC. Hover a parameter for an explanation. "
        "Capsule size requires Rebuild; the other values are applied on the next frame.");

    ImGui::SeparatorText("Capsule geometry");
    ImGui::DragFloat("Radius", &controller->capsuleRadius, 0.01f, 0.05f, 2.0f, "%.3f");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Horizontal radius. A smaller capsule fits doorways but can enter narrow gaps.");
    ImGui::DragFloat("Cylinder height", &controller->capsuleHeight, 0.01f, 0.10f, 4.0f, "%.3f");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Height of the straight capsule section. Total height = cylinder + 2 * radius.");

    ImGui::Checkbox("Automatic center offset", &tuning->autoCalculateCapsuleCenterOffset);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Keeps Transform.position under the feet and calculates the Bullet center automatically.");
    if (tuning->autoCalculateCapsuleCenterOffset)
    {
        ImGui::DragFloat("Foot clearance", &tuning->capsuleFootClearance,
            0.01f, -3.0f, 3.0f, "%.3f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Positive = capsule bottom above the visual foot point. Start with 0.01-0.03.");
    }
    else
    {
        ImGui::DragFloat("Manual center offset Y", &tuning->manualCapsuleCenterOffsetY,
            0.02f, -5.0f, 8.0f, "%.3f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Bullet capsule center relative to Transform.position (the model foot point).");
    }

    const float capsuleHalfHeight = controller->capsuleHeight * 0.5f + controller->capsuleRadius;
    const float configuredOffset = tuning->autoCalculateCapsuleCenterOffset
        ? capsuleHalfHeight + tuning->capsuleFootClearance
        : tuning->manualCapsuleCenterOffsetY;
    const float bottomFromFeet = configuredOffset - capsuleHalfHeight;
    ImGui::Text("Total capsule height: %.3f", controller->capsuleHeight + controller->capsuleRadius * 2.0f);
    ImGui::Text("Capsule bottom relative to feet: %+.3f", bottomFromFeet);
    if (std::abs(bottomFromFeet) > 0.10f)
        ImGui::TextColored(COL_RED, "Warning: capsule and visible feet are strongly misaligned.");

    // Imported character models do not necessarily place their local origin at
    // the soles. This button reads the real mesh AABB and computes the center
    // offset that puts the Bullet capsule bottom at the lowest model vertex.
    auto* characterMesh = registry.GetComponent<MeshRendererComponent>(entity);
    auto* characterTransform = registry.GetComponent<TransformComponent>(entity);
    const bool canAlignToMesh = characterMesh && characterMesh->model && characterTransform;
    ImGui::BeginDisabled(!canAlignToMesh);
    if (ImGui::Button("Align capsule bottom to model feet", ImVec2(-1.0f, 0.0f)))
    {
        XMFLOAT3 modelMin{};
        XMFLOAT3 modelMax{};
        characterMesh->model->GetBoundingBox(&modelMin, &modelMax);

        // Negative scale mirrors the model, so calculate both scaled endpoints
        // instead of assuming scale.y is always positive.
        const float scaledA = modelMin.y * characterTransform->scale.y;
        const float scaledB = modelMax.y * characterTransform->scale.y;
        const float modelFeetFromOrigin = (std::min)(scaledA, scaledB);
        tuning->autoCalculateCapsuleCenterOffset = false;
        tuning->manualCapsuleCenterOffsetY = capsuleHalfHeight + modelFeetFromOrigin;
        CharacterControllerSystem::ForceReconfigure(entity);
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Places the physical capsule bottom on the lowest vertex of the selected character mesh.");

    ImGui::SeparatorText("Movement and step");
    ImGui::DragFloat("Normal step height", &controller->stepHeight, 0.01f, 0.0f, 2.0f, "%.3f");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Permanent step-up sweep. Too large can hit ceilings and climb walls.");
    ImGui::DragFloat("Jump speed", &controller->jumpSpeed, 0.1f, 0.0f, 30.0f, "%.2f");
    ImGui::DragFloat("Gravity", &controller->gravity, 0.1f, 0.0f, 60.0f, "%.2f");
    ImGui::DragFloat("Maximum slope (degrees)", &controller->maxSlope, 0.5f, 0.0f, 89.9f, "%.1f");
    ImGui::DragFloat("Maximum fall speed", &tuning->fallSpeed, 0.5f, 0.1f, 200.0f, "%.1f");

    ImGui::SeparatorText("Normal-step limits");
    ImGui::Checkbox("Clamp normal step height", &tuning->clampNormalStepHeight);
    if (tuning->clampNormalStepHeight)
    {
        ImGui::DragFloat("Minimum step", &tuning->minimumStepHeight, 0.01f, 0.0f, 2.0f, "%.3f");
        ImGui::DragFloat("Maximum step", &tuning->maximumStepHeight, 0.01f, 0.0f, 2.0f, "%.3f");
    }
    ImGui::Checkbox("Migrate legacy step value", &tuning->migrateLegacyStepHeight);
    if (tuning->migrateLegacyStepHeight)
    {
        ImGui::DragFloat("Legacy value", &tuning->legacyStepHeight, 0.01f, 0.0f, 2.0f, "%.3f");
        ImGui::DragFloat("Legacy replacement", &tuning->legacyReplacementStepHeight,
            0.01f, 0.0f, 2.0f, "%.3f");
    }

    ImGui::SeparatorText("Physical low-step solver");
    ImGui::Checkbox("Enable physical step solver", &tuning->automaticStepEnabled);
    if (tuning->automaticStepEnabled)
    {
        ImGui::Checkbox("Predict ledges before contact", &tuning->automaticStepPredictive);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Recommended: removes the visible stop before a low ledge.");
        ImGui::DragFloat("Maximum solved ledge", &tuning->automaticStepHeight, 0.01f, 0.05f, 0.35f, "%.3f");
        if (!tuning->automaticStepPredictive)
            ImGui::DragFloat("Blocked delay", &tuning->automaticStepDelay, 0.01f, 0.0f, 3.0f, "%.2f s");
        ImGui::DragFloat("Solver active display time", &tuning->automaticStepDuration, 0.01f, 0.01f, 3.0f, "%.2f s");
        if (!tuning->automaticStepPredictive)
        {
            ImGui::DragFloat("Retry cooldown", &tuning->automaticStepCooldown, 0.01f, 0.0f, 3.0f, "%.2f s");
            ImGui::DragFloat("Progress distance", &tuning->automaticStepProgressDistance,
                0.005f, 0.001f, 1.0f, "%.3f");
        }
        ImGui::DragFloat("Forward probe", &tuning->automaticStepForwardProbe,
            0.01f, 0.04f, 0.30f, "%.3f");
        ImGui::DragFloat("Downward probe", &tuning->automaticStepDownProbe,
            0.01f, 0.02f, 0.25f, "%.3f");
        ImGui::DragFloat("Landing skin", &tuning->automaticStepSkin,
            0.002f, 0.005f, 0.05f, "%.3f");
        ImGui::TextWrapped("The real capsule is swept up, forward and down. A step is accepted only with headroom and a walkable landing.");
    }

    ImGui::SeparatorText("Bullet advanced");
    ImGui::Checkbox("Force world Y-up", &tuning->forceYAxis);
    ImGui::Checkbox("Use ghost-only sweep", &tuning->useGhostSweepTest);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("OFF is recommended: sweep against the full collision world.");
    ImGui::DragFloat("Maximum penetration depth", &tuning->maxPenetrationDepth,
        0.005f, 0.001f, 0.50f, "%.3f");

    ImGui::SeparatorText("Dynamic body pushing");
    ImGui::Checkbox("Push dynamic bodies", &tuning->pushDynamicBodies);
    ImGui::BeginDisabled(!tuning->pushDynamicBodies);
    ImGui::DragFloat("Push force", &tuning->dynamicBodyPushForce,
        5.0f, 0.0f, 1000.0f, "%.1f N");
    ImGui::DragFloat("Maximum pushable mass", &tuning->dynamicBodyMaxPushableMass,
        1.0f, 0.0f, 10000.0f, "%.1f kg");
    ImGui::DragFloat("Maximum push speed", &tuning->dynamicBodyMaxPushSpeed,
        0.05f, 0.0f, 20.0f, "%.2f m/s");
    ImGui::DragFloat("Push probe distance", &tuning->dynamicBodyPushProbe,
        0.005f, 0.0f, 0.20f, "%.3f m");
    ImGui::EndDisabled();

    const bool runtimeReady = controller->controller && controller->ghostObj && controller->capsuleShape;
    ImGui::SeparatorText("Runtime diagnostics");
    ImGui::TextColored(runtimeReady ? COL_ACCENT : COL_TEXT_DIM,
        "Runtime controller: %s", runtimeReady ? "ready" : "created on Play");
    ImGui::Text("Grounded: %s", controller->isGrounded ? "true" : "false");
    ImGui::Text("Stored center offset Y: %.3f", controller->capsuleCenterOffsetY);
    if (runtimeReady)
    {
        ImGui::Text("Runtime step height: %.3f", controller->controller->getStepHeight());
        ImGui::Text("Geometric ground support: %s",
            CharacterControllerSystem::HasGroundSupport(entity) ? "YES" : "no");
        ImGui::Text("Idle physics lock: %s",
            CharacterControllerSystem::IsIdlePhysicsLocked(entity) ? "LOCKED" : "active");
        ImGui::Text("Physical step solver: %s",
            CharacterControllerSystem::IsAutomaticStepActive(entity) ? "ACTIVE" : "inactive");
    }

    if (ImGui::Button("Apply all runtime values", ImVec2(-1.0f, 0.0f)))
        CharacterControllerSystem::ForceReconfigure(entity);

    if (ImGui::Button("Rebuild capsule from size", ImVec2(-1.0f, 0.0f)))
    {
        if (auto* transform = registry.GetComponent<TransformComponent>(entity))
        {
            CharacterSpawnSystem::Spawn(registry, entity, transform->position);
            CharacterControllerSystem::ForceReconfigure(entity);
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Required after changing Radius or Cylinder height.");

    if (ImGui::Button("Reset safe character defaults", ImVec2(-1.0f, 0.0f)))
    {
        *tuning = CharacterPhysicsTuningComponent{};
        controller->capsuleRadius = 0.40f;
        controller->capsuleHeight = 1.40f;
        controller->capsuleCenterOffsetY = 1.12f;
        controller->stepHeight = 0.35f;
        controller->jumpSpeed = 6.0f;
        controller->gravity = 15.0f;
        controller->maxSlope = 45.0f;
        CharacterControllerSystem::ForceReconfigure(entity);
    }

    ImGui::PopID();
}

EditorUI::~EditorUI() { Shutdown(); }

bool EditorUI::Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context, FileSystem* fs)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Разрешаем растягивать окна за любую границу, а не только за нижний угол,
    // который в Play Mode может оказаться под нижним краем экрана.
    io.ConfigWindowsResizeFromEdges = true;

    m_fileSystem = fs;
    if (m_fileSystem)
    {
        m_currentDir = "";
        m_fileSystem->Refresh();
    }

    m_debugSettings.showColliders = false;
    m_debugSettings.showContacts = false;
    m_debugSettings.showAABB = false;
    m_debugSettings.colorDynamic = { 0.2f, 0.8f, 0.2f, 0.6f };
    m_debugSettings.colorStatic = { 0.8f, 0.2f, 0.2f, 0.6f };

    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();

    m_hwnd = hwnd;
    m_device = device;
    m_context = context;

    s.WindowRounding = 4.0f;
    s.FrameRounding = 3.0f;
    s.PopupRounding = 3.0f;
    s.ScrollbarRounding = 3.0f;
    s.GrabRounding = 3.0f;
    s.WindowBorderSize = 0.0f;
    s.FrameBorderSize = 0.0f;
    s.ItemSpacing = ImVec2(6, 5);
    s.FramePadding = ImVec2(8, 4);
    s.IndentSpacing = 16.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    c[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    c[ImGuiCol_Header] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    c[ImGuiCol_HeaderActive] = COL_ACCENT;
    c[ImGuiCol_Button] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    c[ImGuiCol_ButtonActive] = COL_ACCENT;
    c[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    c[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    c[ImGuiCol_Tab] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    c[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    c[ImGuiCol_TabActive] = COL_ACCENT;
    c[ImGuiCol_Separator] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    c[ImGuiCol_CheckMark] = COL_ACCENT;
    c[ImGuiCol_SliderGrab] = COL_ACCENT;
    c[ImGuiCol_SliderGrabActive] = COL_ACCENT_HOV;

    const float dpiScale = static_cast<float>(GetDpiForWindow(hwnd)) / 96.0f;
    io.FontGlobalScale = 1.0f;

    // Стандартный шрифт Windows содержит кириллицу. Диапазон загружается сразу,
    // поэтому переключение NPC Studio на русский не требует пересборки атласа.
    static const ImWchar editorGlyphRanges[] = {
        0x0020, 0x00FF,
        0x0400, 0x052F,
        0
    };
    constexpr const char* editorFontPath = "C:\\Windows\\Fonts\\segoeui.ttf";
    if (std::filesystem::exists(editorFontPath))
        io.Fonts->AddFontFromFileTTF(editorFontPath, 16.0f * dpiScale, nullptr, editorGlyphRanges);

    if (!ImGui_ImplWin32_Init(hwnd))           return false;
    if (!ImGui_ImplDX11_Init(device, context))
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    m_initialized = true;
    return true;
}

void EditorUI::Shutdown()
{
    if (m_particlePreviewEmitter != INVALID_PARTICLE_EMITTER)
    {
        if (ParticleSystem* particles = ParticleSystem::GetActiveSystem())
            particles->DestroyEmitter(m_particlePreviewEmitter);
        m_particlePreviewEmitter = INVALID_PARTICLE_EMITTER;
    }

    if (!m_initialized)
        return;

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    m_initialized = false;
    m_device = nullptr;
    m_context = nullptr;
    m_hwnd = nullptr;
}

void EditorUI::NewFrame()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void EditorUI::Render()
{
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

bool EditorUI::WantCaptureMouse() const { return ImGui::GetIO().WantCaptureMouse; }
bool EditorUI::WantTextInput() const { return ImGui::GetIO().WantTextInput; }

void EditorUI::SetEditorStatus(const std::string& message)
{
    m_editorStatus = message;
    m_editorStatusUntil = ImGui::GetTime() + 4.0;
}

XMFLOAT3 EditorUI::GetAuthoringPosition(SceneManager& sceneManager) const
{
    if ((m_selectedType == SelectedType::Object ||
        m_selectedType == SelectedType::Light) &&
        m_selectedEntity != INVALID_ENTITY)
    {
        auto& registry = sceneManager.GetECSRegistry();
        if (auto* transform = registry.GetComponent<TransformComponent>(m_selectedEntity))
            return { transform->position.x + 1.0f, transform->position.y, transform->position.z };
        if (auto* area = registry.GetComponent<WorldAreaComponent>(m_selectedEntity))
            return { area->center.x + 1.0f, area->center.y, area->center.z };
    }

    return { m_spawnPos[0], m_spawnPos[1], m_spawnPos[2] };
}

Entity EditorUI::CreateEditorTemplate(SceneManager& sceneManager, QuickCreateKind kind)
{
    auto& registry = sceneManager.GetECSRegistry();
    const XMFLOAT3 position = GetAuthoringPosition(sceneManager);
    Entity entity = INVALID_ENTITY;

    auto spawnLogicalEntity = [&](const char* name, bool interactable)
    {
        SpawnObjectDesc desc;
        desc.name = name;
        desc.position = position;
        desc.addMeshRenderer = false;
        desc.addInteraction = interactable;
        if (interactable)
        {
            desc.interaction.enabled = true;
            desc.interaction.maxDistance = 3.0f;
            desc.interaction.prompt = "Interact";
        }
        return sceneManager.SpawnObject(std::move(desc));
    };

    auto createArea = [&](WorldAreaType type, const char* name, const XMFLOAT3& size)
    {
        Entity areaEntity = sceneManager.CreateWorldArea(type);
        if (auto* area = registry.GetComponent<WorldAreaComponent>(areaEntity))
        {
            area->center = position;
            area->size = size;
        }
        if (auto* areaName = registry.GetComponent<NameComponent>(areaEntity))
            areaName->name = name;
        return areaEntity;
    };

    switch (kind)
    {
    case QuickCreateKind::EmptyEntity:
        entity = spawnLogicalEntity("Empty Entity", false);
        break;
    case QuickCreateKind::InteractionPoint:
        entity = spawnLogicalEntity("Interaction Point", true);
        break;
    case QuickCreateKind::PointLight:
        entity = spawnLogicalEntity("Point Light", false);
        registry.AddComponent<LightComponent>(entity, LightComponent{});
        break;
    case QuickCreateKind::CrewNpc:
        entity = spawnLogicalEntity("Crew NPC", true);
        if (auto* interaction = registry.GetComponent<InteractionComponent>(entity))
            interaction->prompt = "Talk";
        registry.AddComponent<NPCComponent>(entity, NPCComponent{});
        registry.AddComponent<CharacterVitalsComponent>(
            entity, CharacterVitalsComponent{});
        registry.AddComponent<NpcBrainComponent>(entity, NpcBrainComponent{});
        registry.AddComponent<NPCMovementComponent>(entity, NPCMovementComponent{});
        registry.AddComponent<NpcPatrolComponent>(entity, NpcPatrolComponent{});
        registry.AddComponent<CharacterMovement>(entity, CharacterMovement{});
        registry.AddComponent<CharacterController>(entity, CharacterController{});
        registry.AddComponent<CharacterPhysicsTuningComponent>(
            entity, CharacterPhysicsTuningComponent{});
        registry.AddComponent<AnimationComponent>(entity, AnimationComponent{});
        registry.AddComponent<CrewMemberComponent>(entity, CrewMemberComponent{});
        break;
    case QuickCreateKind::CoverPoint:
        entity = spawnLogicalEntity("NPC Cover Point", false);
        registry.AddComponent<NpcCoverPointComponent>(
            entity, NpcCoverPointComponent{});
        break;
    case QuickCreateKind::PatrolPoint:
        entity = spawnLogicalEntity("NPC Patrol Point", false);
        registry.AddComponent<NpcPatrolPointComponent>(
            entity, NpcPatrolPointComponent{});
        break;
    case QuickCreateKind::RiflePickup:
        {
            AssimpModelClass* rifleModel = sceneManager.GetOrCreateModel(
                "assets/model/Rifle_M4_CC0.glb",
                m_device,
                m_context,
                "",
                false);
            if (!rifleModel)
            {
                SetEditorStatus("Could not load assets/model/Rifle_M4_CC0.glb");
                break;
            }

            SpawnObjectDesc rifle;
            rifle.model = rifleModel;
            rifle.name = "Rifle Pickup";
            rifle.position = position;
            rifle.addInteraction = true;
            rifle.interaction.enabled = true;
            rifle.interaction.maxDistance = 3.0f;
            rifle.interaction.prompt = "Pick up rifle";
            rifle.addRigidBody = true;
            rifle.rigidBody.mass = 1.5f;
            rifle.rigidBody.isDynamic = true;
            rifle.rigidBody.shapeType = RigidBodyComponent::Box;
            rifle.rigidBody.useManualSize = true;
            rifle.rigidBody.shapeSize = btVector3(0.35f, 0.18f, 0.90f);
            entity = sceneManager.SpawnObject(std::move(rifle));
            if (entity != INVALID_ENTITY)
            {
                registry.AddComponent<WeaponComponent>(entity, WeaponComponent{});
                registry.AddComponent<WeaponPickupComponent>(
                    entity, WeaponPickupComponent{});
            }
        }
        break;
    case QuickCreateKind::GrassArea:
        entity = createArea(WorldAreaType::Grass, "Grass Area", { 10.0f, 1.0f, 10.0f });
        break;
    case QuickCreateKind::NoGrassArea:
        entity = createArea(WorldAreaType::NoGrass, "No Grass Area", { 10.0f, 1.0f, 10.0f });
        break;
    case QuickCreateKind::ForestArea:
        entity = createArea(WorldAreaType::Forest, "Forest Area", { 20.0f, 2.0f, 20.0f });
        break;
    case QuickCreateKind::SpawnArea:
        entity = createArea(WorldAreaType::Spawn, "Spawn Area", { 4.0f, 2.0f, 4.0f });
        break;
    case QuickCreateKind::TriggerArea:
        entity = spawnLogicalEntity("Trigger Zone", false);
        {
            TriggerZoneComponent trigger;
            trigger.boxHalfExtents = { 1.5f, 1.0f, 1.5f };
            trigger.eventId = "Trigger";
            registry.AddComponent<TriggerZoneComponent>(entity, std::move(trigger));
            registry.AddComponent<TriggerActionComponent>(
                entity, TriggerActionComponent{});
        }
        break;
    }

    if (entity != INVALID_ENTITY)
    {
        const SelectedType selectionType = registry.HasComponent<LightComponent>(entity)
            ? SelectedType::Light
            : SelectedType::Object;
        SetSelection(selectionType, static_cast<int>(entity));
        SetEditorStatus("Created " + EditorEntityDebugLabel(registry, entity));
    }

    return entity;
}

void EditorUI::RenderQuickCreateMenu(SceneManager& sceneManager)
{
    ImGui::TextDisabled("Gameplay");
    if (ImGui::MenuItem("Empty Entity"))
        CreateEditorTemplate(sceneManager, QuickCreateKind::EmptyEntity);
    if (ImGui::MenuItem("Interaction Point"))
        CreateEditorTemplate(sceneManager, QuickCreateKind::InteractionPoint);
    if (ImGui::MenuItem("Point Light"))
        CreateEditorTemplate(sceneManager, QuickCreateKind::PointLight);
    if (ImGui::MenuItem("Crew NPC (logic)"))
        CreateEditorTemplate(sceneManager, QuickCreateKind::CrewNpc);
    if (ImGui::MenuItem("NPC Cover Point"))
        CreateEditorTemplate(sceneManager, QuickCreateKind::CoverPoint);
    if (ImGui::MenuItem("NPC Patrol Point"))
        CreateEditorTemplate(sceneManager, QuickCreateKind::PatrolPoint);
    if (ImGui::MenuItem("Rifle Pickup"))
        CreateEditorTemplate(sceneManager, QuickCreateKind::RiflePickup);

    ImGui::Separator();
    ImGui::TextDisabled("Zones");
    if (ImGui::MenuItem("Trigger Zone"))
        CreateEditorTemplate(sceneManager, QuickCreateKind::TriggerArea);
    if (ImGui::MenuItem("Spawn Area"))
        CreateEditorTemplate(sceneManager, QuickCreateKind::SpawnArea);
    if (ImGui::MenuItem("Grass Area"))
        CreateEditorTemplate(sceneManager, QuickCreateKind::GrassArea);
    if (ImGui::MenuItem("No Grass Area"))
        CreateEditorTemplate(sceneManager, QuickCreateKind::NoGrassArea);
    if (ImGui::MenuItem("Forest Area"))
        CreateEditorTemplate(sceneManager, QuickCreateKind::ForestArea);
}

void EditorUI::RequestDeleteSelection()
{
    if (m_selectedType == SelectedType::None)
        return;

    m_pendingDeleteType = m_selectedType;
    m_pendingDeleteEntity = m_selectedEntity;
    m_pendingDeleteLegacyIndex = m_selectedLegacyIndex;
    m_openDeleteConfirmation = true;
}

void EditorUI::HandleEditorShortcuts(SceneManager& sceneManager)
{
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput)
        return;

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
    {
        if (!m_lastSavePath.empty())
        {
            m_saveRequest.pending = true;
            m_saveRequest.path = m_lastSavePath;
            SetEditorStatus("Scene save requested: " + m_lastSavePath);
        }
        else
        {
            m_openSaveDialog = true;
        }
    }

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false))
        m_openLoadDialog = true;

    if (!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
        RequestDeleteSelection();

    (void)sceneManager;
}

void EditorUI::RenderDeleteConfirmation(SceneManager& sceneManager)
{
    if (m_openDeleteConfirmation)
    {
        ImGui::OpenPopup("Delete selected item?");
        m_openDeleteConfirmation = false;
    }

    ImGui::SetNextWindowSize(ImVec2(390.0f, 150.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(
        "Delete selected item?", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        return;
    }

    std::string itemLabel = "selected item";
    if (m_pendingDeleteType == SelectedType::Sprite)
    {
        itemLabel = "Sprite #" + std::to_string(m_pendingDeleteLegacyIndex);
    }
    else if (m_pendingDeleteEntity != INVALID_ENTITY)
    {
        itemLabel = EditorEntityDebugLabel(
            sceneManager.GetECSRegistry(), m_pendingDeleteEntity);
    }

    ImGui::TextWrapped("Delete %s? This action cannot be undone.", itemLabel.c_str());
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.30f, 0.30f, 1.0f));
    if (ImGui::Button("Delete", ImVec2(120.0f, 0.0f)))
    {
        bool removed = false;
        if (m_pendingDeleteType == SelectedType::Sprite)
        {
            removed = sceneManager.RemoveSprite(m_pendingDeleteLegacyIndex);
        }
        else if (m_pendingDeleteEntity != INVALID_ENTITY)
        {
            removed = sceneManager.RemoveObject(m_pendingDeleteEntity);
        }

        if (removed)
        {
            SetEditorStatus("Deleted " + itemLabel);
            m_selectedType = SelectedType::None;
            m_selectedEntity = INVALID_ENTITY;
            m_selectedLegacyIndex = -1;
        }
        else
        {
            SetEditorStatus("Could not delete " + itemLabel);
        }
        ImGui::CloseCurrentPopup();
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void EditorUI::RenderEditorToolbar(SceneManager& sceneManager)
{
    const ImGuiIO& io = ImGui::GetIO();
    const float editorViewportWidth = (std::max)(200.0f, io.DisplaySize.x - SidebarWidth());
    const float y = (m_menuBarHeight > 0.0f ? m_menuBarHeight : 20.0f) + 8.0f;

    ImGui::SetNextWindowPos(
        ImVec2(editorViewportWidth * 0.5f, y),
        ImGuiCond_Always,
        ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.94f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
    if (ImGui::Begin(
        "##EditorToolbar", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoNav))
    {
        ImGui::PushStyleColor(ImGuiCol_Button, COL_ACCENT);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_ACCENT_HOV);
        if (ImGui::Button("+ Create"))
            ImGui::OpenPopup("##ToolbarCreate");
        ImGui::PopStyleColor(2);
        if (ImGui::BeginPopup("##ToolbarCreate"))
        {
            RenderQuickCreateMenu(sceneManager);
            ImGui::EndPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button(m_lastSavePath.empty() ? "Save As" : "Save"))
        {
            if (m_lastSavePath.empty())
            {
                m_openSaveDialog = true;
            }
            else
            {
                m_saveRequest.pending = true;
                m_saveRequest.path = m_lastSavePath;
                SetEditorStatus("Scene save requested: " + m_lastSavePath);
            }
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(m_selectedType == SelectedType::None);
        if (ImGui::Button("Delete"))
            RequestDeleteSelection();
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::Checkbox("Snap", &m_transformSnapEnabled);
        if (m_transformSnapEnabled)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("Settings"))
                ImGui::OpenPopup("##SnapSettings");
            if (ImGui::BeginPopup("##SnapSettings"))
            {
                ImGui::TextDisabled("Inspector transform snapping");
                ImGui::DragFloat("Position step", &m_positionSnap, 0.01f, 0.01f, 100.0f, "%.2f");
                ImGui::DragFloat("Rotation step", &m_rotationSnapDegrees, 1.0f, 1.0f, 180.0f, "%.0f deg");
                ImGui::DragFloat("Scale step", &m_scaleSnap, 0.01f, 0.01f, 10.0f, "%.2f");
                ImGui::EndPopup();
            }
        }

        if (!m_editorStatus.empty() && ImGui::GetTime() <= m_editorStatusUntil)
        {
            ImGui::SameLine();
            ImGui::TextColored(COL_GREEN, "%s", m_editorStatus.c_str());
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void EditorUI::RenderEntityIdentity(SceneManager& sceneManager, Entity entity)
{
    auto& registry = sceneManager.GetECSRegistry();
    ImGui::PushID(static_cast<int>(entity));
    SectionHeader("Entity");
    ImGui::Spacing();

    ImGui::TextColored(COL_TEXT_DIM, "Entity ID");
    ImGui::SameLine();
    ImGui::Text("%u", entity);

    if (auto* name = registry.GetComponent<NameComponent>(entity))
    {
        char nameBuffer[192]{};
        strncpy_s(nameBuffer, sizeof(nameBuffer), name->name.c_str(), _TRUNCATE);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
            name->name = nameBuffer;
    }
    else if (ImGui::Button("+ Add Name", ImVec2(-1.0f, 0.0f)))
    {
        registry.AddComponent<NameComponent>(entity, NameComponent{ "Entity" });
    }

    bool active = sceneManager.IsObjectActive(entity);
    if (ImGui::Checkbox("Active in scene", &active))
        sceneManager.SetObjectActive(entity, active);

    std::string components;
    auto appendComponent = [&](bool present, const char* label)
    {
        if (!present)
            return;
        if (!components.empty())
            components += "  |  ";
        components += label;
    };
    appendComponent(registry.HasComponent<TransformComponent>(entity), "Transform");
    appendComponent(registry.HasComponent<MeshRendererComponent>(entity), "Mesh");
    appendComponent(registry.HasComponent<RigidBodyComponent>(entity), "Physics");
    appendComponent(registry.HasComponent<InteractionComponent>(entity), "Interaction");
    appendComponent(registry.HasComponent<NPCComponent>(entity), "NPC");
    appendComponent(registry.HasComponent<LightComponent>(entity), "Light");
    appendComponent(registry.HasComponent<TriggerZoneComponent>(entity), "Trigger Zone");
    appendComponent(registry.HasComponent<TriggerActionComponent>(entity), "Trigger Action");
    appendComponent(registry.HasComponent<CrewMemberComponent>(entity), "Crew Member");
    appendComponent(registry.HasComponent<InventoryComponent>(entity), "Inventory");
    appendComponent(registry.HasComponent<WeaponPickupComponent>(entity), "Weapon Pickup");
    appendComponent(registry.HasComponent<WorldAreaComponent>(entity), "World Area");
    appendComponent(registry.HasComponent<RoomComponent>(entity), "Room");
    appendComponent(registry.HasComponent<DoorComponent>(entity), "Door");
    appendComponent(registry.HasComponent<WeaponComponent>(entity), "Weapon");
    if (!components.empty())
        ImGui::TextWrapped("%s", components.c_str());

    ImGui::PopID();
    ImGui::Spacing();
}

void EditorUI::SectionHeader(const char* label)
{
    const float headerH = std::floor(24.0f * EditorUI::Scale());
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
    ImGui::BeginChild(label, ImVec2(-1, headerH), false);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::floor(4.0f * EditorUI::Scale()));
    ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.75f, 1.0f), "  %s", label);
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static bool DragFloat3Colored(const char* id, float v[3], float speed = 0.1f)
{
    bool changed = false;
    ImGui::PushID(id);
    const float w = (ImGui::GetContentRegionAvail().x - 6.0f) / 3.0f;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.30f, 0.08f, 0.08f, 1.0f));
    ImGui::SetNextItemWidth(w);
    if (ImGui::DragFloat("##X", &v[0], speed)) changed = true;
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 2);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.25f, 0.08f, 1.0f));
    ImGui::SetNextItemWidth(w);
    if (ImGui::DragFloat("##Y", &v[1], speed)) changed = true;
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 2);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.13f, 0.30f, 1.0f));
    ImGui::SetNextItemWidth(w);
    if (ImGui::DragFloat("##Z", &v[2], speed)) changed = true;
    ImGui::PopStyleColor();

    ImGui::PopID();
    return changed;
}

void EditorUI::RenderMainMenu(SceneManager& sceneManager)
{
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.08f, 0.08f, 0.08f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.12f, 0.12f, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.06f, 0.53f, 0.98f, 0.25f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.06f, 0.53f, 0.98f, 0.55f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, COL_ACCENT);

    if (ImGui::BeginMainMenuBar())
    {
        m_menuBarHeight = ImGui::GetWindowSize().y;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 8.0f));

        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
        ImGui::Text(" DX Editor ");
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
        ImGui::Text(" | ");
        ImGui::PopStyleColor();

        if (ImGui::BeginMenu("  File  "))
        {
            const bool hasSavePath = !m_lastSavePath.empty();

            ImGui::PushStyleColor(ImGuiCol_Text, COL_GREEN);
            if (ImGui::MenuItem("  Save Scene", "Ctrl+S", false, hasSavePath))
            {
                m_saveRequest.pending = true;
                m_saveRequest.path = m_lastSavePath;
            }
            ImGui::PopStyleColor();

            if (ImGui::MenuItem("  Save As..."))
            {
                if (!m_lastSavePath.empty())
                    strncpy_s(m_savePathBuf, sizeof(m_savePathBuf),
                        m_lastSavePath.c_str(), _TRUNCATE);
                m_openSaveDialog = true;
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.78f, 1.00f, 1.00f));
            if (ImGui::MenuItem("  Load Scene...", "Ctrl+O"))
            {
                if (!m_lastSavePath.empty())
                    strncpy_s(m_loadPathBuf, sizeof(m_loadPathBuf),
                        m_lastSavePath.c_str(), _TRUNCATE);
                m_openLoadDialog = true;
            }
            ImGui::PopStyleColor();

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("  Create  "))
        {
            RenderQuickCreateMenu(sceneManager);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("  Edit  "))
        {
            ImGui::BeginDisabled(m_selectedType == SelectedType::None);
            if (ImGui::MenuItem("Delete Selected", "Delete"))
                RequestDeleteSelection();
            ImGui::EndDisabled();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("  View  "))
        {
            ImGui::MenuItem("Show Inactive Entities", nullptr, &m_showInactiveEntities);
            ImGui::MenuItem("Transform Snapping", nullptr, &m_transformSnapEnabled);
            ImGui::EndMenu();
        }

        {
            auto& registry = sceneManager.GetECSRegistry();
            char status[160];
            snprintf(status, sizeof(status), " %zu entities  |  %zu meshes  |  %zu lights  |  %d sprites  ",
                registry.GetEntityCount(),
                sceneManager.GetObjectCount(),
                registry.GetComponents<LightComponent>().GetCount(),
                sceneManager.GetSpriteCount());

            const float textW = ImGui::CalcTextSize(status).x;
            const float barW = ImGui::GetContentRegionAvail().x;
            if (barW > textW + 10.0f)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + barW - textW);

            ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 1.0f), "%s", status);
        }

        ImGui::PopStyleVar(1);
        ImGui::EndMainMenuBar();
    }

    ImGui::PopStyleColor(5);

    if (m_openSaveDialog) { ImGui::OpenPopup("##SaveDialog"); m_openSaveDialog = false; }
    if (m_openLoadDialog) { ImGui::OpenPopup("##LoadDialog"); m_openLoadDialog = false; }

    RenderSaveDialog();
    RenderLoadDialog();
}

// -----------------------------------------------------------------------------
// Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Save AsР С—РЎвЂ”Р вЂ¦
// -----------------------------------------------------------------------------
void EditorUI::RenderSaveDialog()
{
    const ImVec2 dialogSz = ImVec2(std::floor(420.0f * ScaleX()),
        std::floor(160.0f * ScaleY()));

    ImGui::SetNextWindowSize(dialogSz, ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        ImVec2((ImGui::GetIO().DisplaySize.x - dialogSz.x) * 0.5f,
            (ImGui::GetIO().DisplaySize.y - dialogSz.y) * 0.5f),
        ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.10f, 0.10f, 0.10f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.06f, 0.53f, 0.98f, 1.00f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));

    if (ImGui::BeginPopupModal("##SaveDialog", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, COL_GREEN);
        ImGui::Text("  Save Scene As");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.65f, 1.0f), "File path:");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##SavePath", m_savePathBuf, sizeof(m_savePathBuf));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float btnW = std::floor(120.0f * ScaleX());

        ImGui::PushStyleColor(ImGuiCol_Button, COL_GREEN);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_GREEN_HOV);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.08f, 0.45f, 0.18f, 1.0f));
        if (ImGui::Button("  Save  ", ImVec2(btnW, 0)))
        {
            m_saveRequest.pending = true;
            m_saveRequest.path = m_savePathBuf;
            m_lastSavePath = m_savePathBuf;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0, 8.0f);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.32f, 0.32f, 0.32f, 1.0f));
        if (ImGui::Button("  Cancel  ", ImVec2(btnW, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::PopStyleColor(2);

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(1);
    ImGui::PopStyleColor(2);
}

// -----------------------------------------------------------------------------
// Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Load SceneР С—РЎвЂ”Р вЂ¦
// -----------------------------------------------------------------------------
void EditorUI::RenderLoadDialog()
{
    const ImVec2 dialogSz = ImVec2(std::floor(420.0f * ScaleX()),
        std::floor(160.0f * ScaleY()));

    ImGui::SetNextWindowSize(dialogSz, ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        ImVec2((ImGui::GetIO().DisplaySize.x - dialogSz.x) * 0.5f,
            (ImGui::GetIO().DisplaySize.y - dialogSz.y) * 0.5f),
        ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.10f, 0.10f, 0.10f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.06f, 0.53f, 0.98f, 1.00f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));

    if (ImGui::BeginPopupModal("##LoadDialog", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.78f, 1.00f, 1.00f));
        ImGui::Text("  Load Scene");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.65f, 1.0f), "File path:");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##LoadPath", m_loadPathBuf, sizeof(m_loadPathBuf));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float btnW = std::floor(120.0f * ScaleX());

        ImGui::PushStyleColor(ImGuiCol_Button, COL_ACCENT);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_ACCENT_HOV);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.03f, 0.35f, 0.72f, 1.0f));
        if (ImGui::Button("  Load  ", ImVec2(btnW, 0)))
        {
            m_loadRequest.pending = true;
            m_loadRequest.path = m_loadPathBuf;
            m_lastSavePath = m_loadPathBuf;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0, 8.0f);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.32f, 0.32f, 0.32f, 1.0f));
        if (ImGui::Button("  Cancel  ", ImVec2(btnW, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::PopStyleColor(2);

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(1);
    ImGui::PopStyleColor(2);
}

//=============================================================================
// Update Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
//=============================================================================
void EditorUI::Update(SceneManager& sceneManager)
{
    NewFrame();
    HandleEditorShortcuts(sceneManager);
    RenderMainMenu(sceneManager);
    RenderEditorToolbar(sceneManager);
    RenderSidebar(sceneManager);
    RenderCompartmentEditor(sceneManager);
    RenderDetailsPanel(sceneManager);
    RenderScenePreview();
    RenderAssetLoadingPanel(sceneManager);
    EditorDebug::RenderObjectDebugWindow(sceneManager, m_selectedEntity, m_showObjectDebugWindow);
    RenderDeleteConfirmation(sceneManager);
}

void EditorUI::RenderAssetLoadingPanel(SceneManager& sceneManager)
{
    const AsyncShipImportSnapshot ship = sceneManager.GetAsyncShipImportSnapshot();
    std::vector<MeshLoadSnapshot> meshLoads;
    if (auto* assets = sceneManager.GetAssetLoadSystem())
        meshLoads = assets->GetLoadSnapshots();

    const bool hasVisibleMeshLoad = std::any_of(
        meshLoads.begin(), meshLoads.end(), [](const MeshLoadSnapshot& load)
        {
            return load.state == AssetState::Loading ||
                load.state == AssetState::CpuLoaded ||
                load.state == AssetState::Error;
        });

    if (!ship.active && !ship.failed && !hasVisibleMeshLoad)
        return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
            viewport->WorkPos.y + 42.0f),
        ImGuiCond_Always,
        ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f), ImGuiCond_Always);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings;

    if (!ImGui::Begin("Asset Loading", nullptr, flags))
    {
        ImGui::End();
        return;
    }

    const char spinner[] = { '|', '/', '-', '\\' };
    const int spinnerIndex = static_cast<int>(ImGui::GetTime() * 8.0) & 3;

    if (ship.active || ship.failed)
    {
        ImGui::Text("%c Ship", ship.active ? spinner[spinnerIndex] : '!');
        ImGui::TextWrapped("%s", ship.path.c_str());
        ImGui::ProgressBar(
            std::clamp(ship.progress, 0.0f, 1.0f),
            ImVec2(-1.0f, 0.0f));
        ImGui::TextWrapped("%s", ship.stage.c_str());
        if (!ship.currentItem.empty())
            ImGui::TextWrapped("Current: %s", ship.currentItem.c_str());
        if (ship.totalItems > 0)
            ImGui::Text("Items: %zu / %zu", ship.completedItems, ship.totalItems);
        if (ship.failed)
            ImGui::TextColored(COL_RED, "Error: %s", ship.error.c_str());
        ImGui::Separator();
    }

    for (const auto& load : meshLoads)
    {
        if (load.state == AssetState::Ready || load.state == AssetState::Unloaded)
            continue;

        ImGui::Text("%c Model", load.state == AssetState::Error ? '!' : spinner[spinnerIndex]);
        ImGui::TextWrapped("%s", load.path.c_str());
        ImGui::ProgressBar(
            std::clamp(load.progress, 0.0f, 1.0f),
            ImVec2(-1.0f, 0.0f));
        ImGui::TextWrapped("%s", load.stage.c_str());
        if (load.totalMeshes > 0)
            ImGui::Text("Meshes: %u / %u", load.processedMeshes, load.totalMeshes);
        if (load.state == AssetState::Error)
            ImGui::TextColored(COL_RED, "Error: %s", load.error.c_str());
    }

    ImGui::End();
}

// РћС‚РґРµР»СЊРЅРѕРµ РєРѕРјРїР°РєС‚РЅРѕРµ РѕРєРЅРѕ СѓРїСЂР°РІР»РµРЅРёСЏ NPC РІ Play mode.
//
// Р’Р°Р¶РЅРѕРµ РїСЂР°РІРёР»Рѕ UI: РєРЅРѕРїРєРё РЅРµ РјРµРЅСЏСЋС‚ state, walkDirection РёР»Рё repairTarget
// РЅР°РїСЂСЏРјСѓСЋ. РћРЅРё РѕР±СЂР°С‰Р°СЋС‚СЃСЏ С‚РѕР»СЊРєРѕ Рє NpcCommandSystem. Р‘Р»Р°РіРѕРґР°СЂСЏ СЌС‚РѕРјСѓ РєРѕРјР°РЅРґР° РёР·
// UI РїСЂРѕС…РѕРґРёС‚ С‚РѕС‚ Р¶Рµ РїСѓС‚СЊ, С‡С‚Рѕ Р±СѓРґСѓС‰Р°СЏ РєРѕРјР°РЅРґР° РєРІРµСЃС‚Р°/РєРѕРЅСЃРѕР»Рё/СЃРµС‚Рё, Рё РІСЃРµРіРґР°
// РїРѕР»СѓС‡Р°РµС‚ РїСЂР°РІРёР»СЊРЅС‹Р№ РїСЂРёРѕСЂРёС‚РµС‚ СЃ РїРѕР»РЅС‹Рј СЃР±СЂРѕСЃРѕРј СЃС‚Р°СЂРѕРіРѕ РјР°СЂС€СЂСѓС‚Р°.
static void RenderNpcPlayDebugWindow(SceneManager& sceneManager, Entity selectedEntity)
{
    auto& registry = sceneManager.GetECSRegistry();
    auto* brain = registry.GetComponent<NpcBrainComponent>(selectedEntity);
    if (!brain || !registry.HasComponent<NPCComponent>(selectedEntity))
        return;
    auto* combat =
        registry.GetComponent<NPCCombatComponent>(selectedEntity);

    ImGui::SetNextWindowSize(ImVec2(390.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("NPC Debug Control"))
    {
        ImGui::End();
        return;
    }

    ImGui::TextColored(
        ImVec4(0.35f, 0.85f, 1.0f, 1.0f),
        "UI under cursor | RMB outside UI: look | WASD + R/F: fly");
    ImGui::Separator();

    // Р’РµСЂС…РЅСЏСЏ С‡Р°СЃС‚СЊ РѕРєРЅР° РЅРёС‡РµРіРѕ РЅРµ СѓРїСЂР°РІР»СЏРµС‚ вЂ” С‚РѕР»СЊРєРѕ РїРѕРєР°Р·С‹РІР°РµС‚ "С‡С‘СЂРЅС‹Р№ СЏС‰РёРє"
    // РјРѕР·РіР°: С‡С‚Рѕ РїСЂРёРєР°Р·Р°РЅРѕ, С‡С‚Рѕ СЂРµР°Р»СЊРЅРѕ РІС‹РїРѕР»РЅСЏРµС‚СЃСЏ Рё РіРґРµ РІРѕР·РЅРёРєР»Р° Р±Р»РѕРєРёСЂРѕРІРєР°.
    ImGui::Text("NPC: %s", EditorEntityDebugLabel(registry, selectedEntity).c_str());
    ImGui::Text("State: %s", NpcStateName(brain->state));
    ImGui::Text("Command: %s", NpcCommandName(brain->command.type));
    ImGui::TextWrapped("Status: %s", brain->status.c_str());
    ImGui::Text("Room: %s", EditorEntityDebugLabel(registry, brain->currentRoom).c_str());
    ImGui::Text("Door: %s", EditorEntityDebugLabel(registry, brain->activeDoor).c_str());
    if (const auto* transform = registry.GetComponent<TransformComponent>(selectedEntity))
    {
        ImGui::Text("NPC pos: %.2f  %.2f  %.2f",
            transform->position.x, transform->position.y, transform->position.z);
    }
    ImGui::SeparatorText("Combat diagnostics");

    if (combat)
    {
        ImGui::DragFloat(
            "See range##npc_play_combat",
            &combat->seeRange,
            0.1f, 0.0f, 1000.0f);

        ImGui::DragFloat(
            "Field of view##npc_play_combat",
            &combat->fieldOfViewDegrees,
            1.0f, 0.0f, 360.0f, "%.1f deg");

        ImGui::DragFloat(
            "Close awareness##npc_play_combat",
            &combat->closeAwarenessRange,
            0.05f, 0.0f, 10.0f, "%.2f m");

        ImGui::DragFloat(
            "Attack range##npc_play_combat",
            &combat->attackRange,
            0.1f, 0.0f, 1000.0f);

        ImGui::DragFloat(
            "Attack cooldown##npc_play_combat",
            &combat->attackCooldown,
            0.05f, 0.0f, 60.0f);

        ImGui::DragFloat(
            "Lose target delay##npc_play_combat",
            &combat->loseTargetDelay,
            0.05f, 0.0f, 60.0f);
        ImGui::DragFloat("Chase resume margin##npc_play_combat",
            &combat->chaseResumeMargin, 0.05f, 0.0f, 5.0f, "%.2f m");
        ImGui::DragFloat("Guard enter delay##npc_play_combat",
            &combat->stanceEnterDelay, 0.05f, 0.0f, 5.0f, "%.2f s");
        ImGui::DragFloat("Prediction time##npc_play_combat",
            &combat->predictionSeconds, 0.05f, 0.0f, 3.0f, "%.2f s");
        ImGui::DragFloat("Shooting range##npc_play_combat",
            &combat->rangedAttackRange, 0.25f, 0.1f, 1000.0f, "%.1f m");
        ImGui::DragFloat("Aim delay##npc_play_combat",
            &combat->rangedAimDelay, 0.05f, 0.0f, 10.0f, "%.2f s");
        ImGui::Text("Ranged: %s", combat->rangedStatus.c_str());
        ImGui::DragFloat("Max prediction distance##npc_play_combat",
            &combat->maxPredictionDistance, 0.1f, 0.0f, 10.0f, "%.2f m");
        ImGui::DragFloat("Prediction travel timeout##npc_play_combat",
            &combat->predictionTravelTimeout, 0.1f, 0.0f, 30.0f, "%.1f s");
        ImGui::Text("Search: %s", combat->investigatingPrediction ? "Checking estimate" : "Waiting");
        ImGui::Text("Last seen: %.2f %.2f %.2f", combat->lastKnownPosition.x,
            combat->lastKnownPosition.y, combat->lastKnownPosition.z);

        ImGui::Text(
            "Target: %s",
            EditorEntityDebugLabel(
                registry, combat->targetEntity).c_str());

        ImGui::Text(
            "Time since seen: %.2f",
            combat->timeSinceTargetSeen);

        ImGui::Text(
            "Attack timer: %.2f",
            combat->attackTimer);
    }
    else
    {
        ImGui::TextDisabled("NPC Combat Component is not attached");
    }

    ImGui::SeparatorText("Door transition diagnostics");
    if (brain->debugDoorSnapshotCaptured)
    {
        ImGui::TextColored(
            ImVec4(1.0f, 0.65f, 0.15f, 1.0f),
            "Frozen stuck snapshot (stable):");
        ImGui::TextWrapped("%s", brain->debugDoorSnapshot.c_str());
        if (ImGui::Button("Clear frozen snapshot"))
        {
            brain->debugDoorSnapshotCaptured = false;
            brain->debugDoorSnapshot.clear();
        }
        ImGui::Separator();
    }
    ImGui::TextDisabled("Live values below may change every frame");
    ImGui::Text("Phase: %s", brain->debugDoorPhase.c_str());
    ImGui::Text("Target room: %s",
        EditorEntityDebugLabel(registry, brain->navigationTargetRoom).c_str());
    ImGui::Text("Door destination: %s",
        EditorEntityDebugLabel(registry, brain->debugDoorDestinationRoom).c_str());
    ImGui::Text("Route: %d / %d  dirty=%s",
        static_cast<int>(brain->doorRouteIndex),
        static_cast<int>(brain->doorRoute.size()),
        brain->routeDirty ? "yes" : "no");
    ImGui::Text("Portal collision bypass: %s",
        brain->portalCollisionIgnoredDoor == INVALID_ENTITY
            ? "inactive"
            : EditorEntityDebugLabel(
                registry, brain->portalCollisionIgnoredDoor).c_str());
    ImGui::Text("Door XYZ:     %.2f  %.2f  %.2f",
        brain->debugDoorPosition.x, brain->debugDoorPosition.y, brain->debugDoorPosition.z);
    ImGui::Text("Approach XYZ: %.2f  %.2f  %.2f",
        brain->debugDoorApproach.x, brain->debugDoorApproach.y, brain->debugDoorApproach.z);
    ImGui::Text("Exit XYZ:     %.2f  %.2f  %.2f  valid=%s",
        brain->debugDoorExit.x, brain->debugDoorExit.y, brain->debugDoorExit.z,
        brain->debugHasDoorExit ? "yes" : "no");
    ImGui::Text("Distances door/approach: %.2f / %.2f",
        brain->debugDistanceToDoor, brain->debugDistanceToApproach);
    ImGui::Text("Beyond: %.2f / %.2f   open=%.2f",
        brain->debugProgressBeyondDoor,
        brain->debugRequiredExitProgress,
        brain->debugDoorOpenProgress);
    if (const auto* movement = registry.GetComponent<CharacterMovement>(selectedEntity))
    {
        ImGui::Text("Move: %s speed=%.2f dir=(%.2f, %.2f)",
            movement->isMoving ? "yes" : "no", movement->currentSpeed,
            movement->walkDirection.x, movement->walkDirection.z);
    }
    ImGui::Separator();
    const NpcNavMeshDebugInfo navInfo = NpcNavMeshSystem::GetDebugInfo(selectedEntity);
    ImGui::Text("NavMesh: %s", navInfo.compiledIn ? "enabled" : "disabled");
    if (navInfo.floorOnlyFallback)
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.12f, 1.0f), "NAVMESH FALLBACK - obstacles ignored");
    ImGui::TextWrapped("Nav status: %s", navInfo.status.c_str());
    ImGui::Text("Nav polygons/path: %d / %d (next %d)",
        navInfo.polygonCount, navInfo.waypointCount, navInfo.currentWaypoint);

    NpcNavMeshDebugDrawSettings& navDraw = NpcNavMeshSystem::GetDebugDrawSettings();
    ImGui::Checkbox("Draw NavMesh in world", &navDraw.enabled);
    if (navDraw.enabled)
    {
        ImGui::Indent();
        ImGui::Checkbox("Mesh polygons", &navDraw.drawMesh);
        ImGui::Checkbox("Calculated path", &navDraw.drawPath);
        ImGui::Checkbox("Obstacle AABBs", &navDraw.drawObstacleAabbs);
        ImGui::DragFloat("Debug height", &navDraw.heightOffset, 0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::TextDisabled("Cyan=mesh, yellow=path, orange=dynamic avoid, red=AABB");
        ImGui::Unindent();
    }
    if (!brain->blockedReason.empty())
        ImGui::TextColored(COL_RED, "Blocked: %s", brain->blockedReason.c_str());

    ImGui::ProgressBar(brain->repairScore, ImVec2(-1.0f, 0.0f), "Utility repair score");
    if (brain->state == NpcState::Repairing)
        ImGui::ProgressBar(brain->repairProgress, ImVec2(-1.0f, 0.0f), "Repair progress");

    // Р’С‹РєР»СЋС‡РµРЅРёРµ manual control РѕРґРЅРѕРІСЂРµРјРµРЅРЅРѕ РІС‹Р·С‹РІР°РµС‚ ResumeAutomatic. Р•СЃР»Рё Р±С‹
    // РјС‹ С‚РѕР»СЊРєРѕ СЃРїСЂСЏС‚Р°Р»Рё РєРЅРѕРїРєРё, РїРѕСЃР»РµРґРЅСЏСЏ РєРѕРјР°РЅРґР° Stop/Wander РѕСЃС‚Р°Р»Р°СЃСЊ Р±С‹ Р°РєС‚РёРІРЅР°
    // РЅР°РІСЃРµРіРґР°, С‡С‚Рѕ РІС‹РіР»СЏРґРµР»Рѕ Р±С‹ РєР°Рє Р·Р°РІРёСЃС€РёР№ AI.
    const bool debugWasEnabled = brain->debugControlEnabled;
    ImGui::Checkbox("Enable manual control", &brain->debugControlEnabled);
    if (debugWasEnabled && !brain->debugControlEnabled)
        NpcCommandSystem::ResumeAutomatic(*brain);

    // BeginDisabled Р±Р»РѕРєРёСЂСѓРµС‚ С‚РѕР»СЊРєРѕ РІРІРѕРґ РїРѕР»СЊР·РѕРІР°С‚РµР»СЏ. РђРІС‚РѕРјР°С‚РёС‡РµСЃРєРёР№ Utility AI
    // РїСЂРѕРґРѕР»Р¶Р°РµС‚ РѕР±РЅРѕРІР»СЏС‚СЊСЃСЏ РЅРµР·Р°РІРёСЃРёРјРѕ РѕС‚ С‚РѕРіРѕ, РѕС‚РєСЂС‹С‚Рѕ Р»Рё СЌС‚Рѕ РѕРєРЅРѕ.
    ImGui::BeginDisabled(!brain->debugControlEnabled);
    const float halfWidth = ImGui::GetContentRegionAvail().x * 0.5f - 2.0f;
    if (ImGui::Button("Stop", ImVec2(halfWidth, 0.0f)))
        NpcCommandSystem::Stop(*brain);
    ImGui::SameLine();
    if (ImGui::Button("Resume auto", ImVec2(-1.0f, 0.0f)))
        NpcCommandSystem::ResumeAutomatic(*brain);

    if (ImGui::Button("Wander", ImVec2(halfWidth, 0.0f)))
        NpcCommandSystem::Wander(*brain);
    ImGui::SameLine();
    if (ImGui::Button("Force repath", ImVec2(-1.0f, 0.0f)))
    {
        NpcCommandSystem::ForceRepath(*brain);
        NpcNavMeshSystem::InvalidatePath(selectedEntity);
    }

    // Р’ combo С…СЂР°РЅСЏС‚СЃСЏ РЅР°СЃС‚РѕСЏС‰РёРµ Entity РєРѕРјРЅР°С‚. РўРµРєСЃС‚РѕРІРѕРµ РёРјСЏ РёСЃРїРѕР»СЊР·СѓРµС‚СЃСЏ Р»РёС€СЊ
    // РєР°Рє РїРѕРґРїРёСЃСЊ: РјР°СЂС€СЂСѓС‚ РІСЃРµРіРґР° СЃС‚СЂРѕРёС‚СЃСЏ РїРѕ DoorComponent.roomA/roomB Entity.
    if (ImGui::Button("Rebuild all NavMeshes", ImVec2(-1.0f, 0.0f)))
    {
        // РЎРµС‚РєРё РєРѕРјРЅР°С‚ РєРµС€РёСЂСѓСЋС‚СЃСЏ РѕС‚РґРµР»СЊРЅРѕ РѕС‚ РїСѓС‚РµР№ NPC. Р­С‚Р° РєРЅРѕРїРєР° РЅСѓР¶РЅР° РїРѕСЃР»Рµ
        // РёР·РјРµРЅРµРЅРёСЏ Transform РєРѕР»Р»Р°Р№РґРµСЂР° РёР»Рё РіСЂР°РЅРёС† РєРѕРјРЅР°С‚С‹ РїСЂСЏРјРѕ РІ Play mode.
        NpcNavMeshSystem::InvalidateAll();
        NpcCommandSystem::ForceRepath(*brain);
    }

    const std::string selectedRoom = EditorEntityDebugLabel(registry, brain->debugSelectedRoom);
    if (ImGui::BeginCombo("Room", selectedRoom.c_str()))
    {
        for (auto& [roomEntity, room] : registry.GetComponents<RoomComponent>())
        {
            const bool selected = brain->debugSelectedRoom == roomEntity;
            const std::string label = room.roomName + " [" + std::to_string(roomEntity) + "]";
            if (ImGui::Selectable(label.c_str(), selected))
                brain->debugSelectedRoom = roomEntity;
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::BeginDisabled(brain->debugSelectedRoom == INVALID_ENTITY);
    if (ImGui::Button("Go to selected room", ImVec2(-1.0f, 0.0f)))
        NpcCommandSystem::MoveToRoom(*brain, brain->debugSelectedRoom);
    ImGui::EndDisabled();

    // РљРѕРѕСЂРґРёРЅР°С‚С‹ РёРјРµРЅРЅРѕ РјРёСЂРѕРІС‹Рµ. Р”Р»СЏ Р»РѕРєР°Р»СЊРЅРѕР№ С‚РѕС‡РєРё РґРѕС‡РµСЂРЅРµРіРѕ РѕР±СЉРµРєС‚Р° СЃРЅР°С‡Р°Р»Р°
    // РЅСѓР¶РЅРѕ РїСЂРёРјРµРЅРёС‚СЊ РµРіРѕ world transform.
    ImGui::DragFloat3("World target", &brain->debugWorldTarget.x, 0.1f);
    if (ImGui::Button("Go to world target", ImVec2(-1.0f, 0.0f)))
        NpcCommandSystem::MoveToPosition(*brain, brain->debugWorldTarget);

    // Entity ID РїРѕР·РІРѕР»СЏРµС‚ С‚РµСЃС‚РёСЂРѕРІР°С‚СЊ Follow/Repair Р±РµР· СЃРїРµС†РёР°Р»СЊРЅРѕР№ gameplay UI.
    // РќРµРІРµСЂРЅС‹Р№ ID РЅРµ РІС‹Р·С‹РІР°РµС‚ crash: StateSystem РїРµСЂРµР№РґС‘С‚ РІ Blocked Рё РѕР±СЉСЏСЃРЅРёС‚
    // РѕС‚СЃСѓС‚СЃС‚РІСѓСЋС‰РёР№ РєРѕРјРїРѕРЅРµРЅС‚ РІ blockedReason/status.
    ImGui::InputScalar("Target entity", ImGuiDataType_U32, &brain->debugTargetEntity);
    if (ImGui::Button("Follow entity", ImVec2(halfWidth, 0.0f)))
        NpcCommandSystem::FollowEntity(*brain, brain->debugTargetEntity);
    ImGui::SameLine();
    if (ImGui::Button("Repair breach entity", ImVec2(-1.0f, 0.0f)))
        NpcCommandSystem::RepairEntity(*brain, brain->debugTargetEntity);

    Entity player = INVALID_ENTITY;
    registry.ForEach<PlayerGameTag>([&](Entity candidate, PlayerGameTag&)
        {
            if (player == INVALID_ENTITY)
                player = candidate;
        });
    ImGui::BeginDisabled(player == INVALID_ENTITY);
    if (ImGui::Button("Follow player", ImVec2(halfWidth, 0.0f)))
        NpcCommandSystem::FollowEntity(*brain, player);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(brain->repairTarget == INVALID_ENTITY);
    if (ImGui::Button("Repair nearest", ImVec2(-1.0f, 0.0f)))
        NpcCommandSystem::RepairEntity(*brain, brain->repairTarget);
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    ImGui::End();
}

static void RenderNpcCombatLab(
    ECSRegistry& registry,
    Entity npc,
    bool russian)
{
    const auto L = [&](const char* english, const char* russianText)
    {
        return russian ? russianText : english;
    };

    if (npc == INVALID_ENTITY || !registry.HasComponent<NPCComponent>(npc))
    {
        ImGui::TextDisabled("%s", L(
            "Select one NPC above to use Combat Lab.",
            "Выберите одного NPC выше для работы с Combat Lab."));
        return;
    }

    auto* modules = registry.GetComponent<NpcCombatModulesComponent>(npc);
    if (!modules)
    {
        ImGui::TextWrapped("%s", L(
            "This NPC has no modular combat configuration.",
            "У этого NPC ещё нет модульной конфигурации боя."));
        if (ImGui::Button(L(
            "Add Minimal Duel configuration",
            "Добавить конфигурацию «Минимальная дуэль»")))
        {
            NpcCombatDebugCommandSystem::ClearForcedRuntime(registry, npc);
            NpcCombatModulesComponent initial;
            initial.ApplyPreset(NpcCombatModulePreset::MinimalDuel);
            registry.AddComponent<NpcCombatModulesComponent>(npc, initial);
            NpcSimulationStudio::step = true;
        }
        return;
    }

    const auto applyPreset = [&](NpcCombatModulePreset preset)
    {
        NpcCombatDebugCommandSystem::ClearForcedRuntime(registry, npc);
        modules->ApplyPreset(preset);
        NpcSimulationStudio::step = true;
    };

    ImGui::SeparatorText(L("Presets", "Профили"));
    if (ImGui::Button(L("Manual sandbox", "Ручная песочница")))
        applyPreset(NpcCombatModulePreset::ManualSandbox);
    ImGui::SameLine();
    if (ImGui::Button(L("Minimal duel", "Минимальная дуэль")))
        applyPreset(NpcCombatModulePreset::MinimalDuel);
    ImGui::SameLine();
    if (ImGui::Button(L("Cover + peek", "Укрытие + peek")))
        applyPreset(NpcCombatModulePreset::CoverAndPeek);
    ImGui::SameLine();
    if (ImGui::Button(L("Full individual", "Полный одиночный")))
        applyPreset(NpcCombatModulePreset::FullIndividual);

    if (ImGui::Button(L(
        "Copy these modules to all combat NPCs",
        "Скопировать модули всем боевым NPC")))
    {
        std::vector<Entity> combatNpcs;
        registry.ForEach<NPCCombatComponent>(
            [&](Entity otherNpc, NPCCombatComponent&)
            {
                if (otherNpc != npc)
                    combatNpcs.push_back(otherNpc);
            });
        for (Entity otherNpc : combatNpcs)
        {
            NpcCombatDebugCommandSystem::ClearForcedRuntime(
                registry,
                otherNpc);
            if (auto* other =
                registry.GetComponent<NpcCombatModulesComponent>(otherNpc))
            {
                const std::uint32_t previousMask = other->EnabledMask();
                *other = *modules;
                other->ClearDebugOverride();
                other->runtimePreviousMask = previousMask;
                other->runtimeMaskInitialized = true;
            }
            else
            {
                NpcCombatModulesComponent copy = *modules;
                copy.ClearDebugOverride();
                copy.runtimePreviousMask = 0;
                copy.runtimeMaskInitialized = false;
                registry.AddComponent<NpcCombatModulesComponent>(
                    otherNpc,
                    copy);
            }
        }
        NpcSimulationStudio::step = true;
    }

    ImGui::SeparatorText(L("Core automatic modules", "Базовые автоматические модули"));
    const auto moduleCheckbox = [&](const char* label, bool* value)
    {
        if (ImGui::Checkbox(label, value))
            NpcSimulationStudio::step = true;
    };
    moduleCheckbox(L("Perception", "Восприятие"), &modules->perception);
    moduleCheckbox(L("Engagement / attack stance", "Вход в боевую стойку"),
        &modules->engagement);
    moduleCheckbox(L("Pursuit", "Преследование"), &modules->pursuit);
    moduleCheckbox(L("Stationary fire", "Стрельба с места"),
        &modules->stationaryFire);

    ImGui::SeparatorText(L("Advanced automatic modules", "Продвинутые автоматические модули"));
    moduleCheckbox(L("Search after contact loss", "Поиск после потери цели"),
        &modules->search);
    moduleCheckbox(L("Automatic tactical style", "Автоматический выбор тактики"),
        &modules->automaticTactics);
    moduleCheckbox(L("Cover selection", "Выбор укрытия"),
        &modules->coverSelection);
    moduleCheckbox(L("Peek cycle", "Цикл выглядывания"),
        &modules->peekCycle);
    moduleCheckbox(L("Reposition after peek", "Смена позиции после peek"),
        &modules->coverReposition);
    moduleCheckbox(L("Dynamic cover provider", "Динамический поиск укрытий"),
        &modules->dynamicCover);
    moduleCheckbox(L("Moving fire", "Стрельба на ходу"),
        &modules->movingFire);
    moduleCheckbox(L("Incoming-fire reaction", "Реакция на входящий огонь"),
        &modules->incomingFireReaction);
    moduleCheckbox(L("Squad tactics (pipeline parked)", "Групповая тактика (пайплайн отключён)"),
        &modules->squadTactics);

    if (modules->peekCycle && !modules->coverSelection)
    {
        ImGui::TextColored(COL_ORANGE, "%s", L(
            "Peek automation is blocked until Cover selection is enabled.",
            "Автоматический peek заблокирован, пока выключен выбор укрытия."));
    }
    if (modules->dynamicCover && !modules->coverSelection)
    {
        ImGui::TextColored(COL_ORANGE, "%s", L(
            "Dynamic cover is a provider and needs Cover selection.",
            "Динамический поиск — поставщик и требует модуль выбора укрытия."));
    }
    if (modules->squadTactics)
    {
        ImGui::TextColored(COL_ORANGE, "%s", L(
            "The squad coordinator remains globally parked until individual combat is stable.",
            "Координатор отряда пока глобально отключён до стабилизации индивидуального боя."));
    }

    const auto* combat = registry.GetComponent<NPCCombatComponent>(npc);
    const auto* brain = registry.GetComponent<NpcBrainComponent>(npc);
    const auto* tactical = registry.GetComponent<NpcTacticalComponent>(npc);

    ImGui::SeparatorText(L("Live pipeline", "Живой конвейер"));
    ImGui::Text(L("State: %s", "Состояние: %s"),
        brain ? NpcStateName(brain->state) : "missing brain");
    ImGui::Text(L("Target: %u | visible: %s", "Цель: %u | видна: %s"),
        combat ? static_cast<unsigned>(combat->targetEntity) : 0u,
        combat && combat->targetVisible ? L("yes", "да") : L("no", "нет"));
    if (tactical)
    {
        ImGui::Text(L(
            "Cover: %s | moving to peek: %s | exposed: %s",
            "Укрытие: %s | выход к peek: %s | открыт: %s"),
            (tactical->reservedCover != INVALID_ENTITY || tactical->dynamicCoverActive)
                ? L("assigned", "назначено") : L("none", "нет"),
            tactical->movingToPeek ? L("yes", "да") : L("no", "нет"),
            tactical->exposed ? L("yes", "да") : L("no", "нет"));
    }
    ImGui::Text(L("Forced dodge: %s", "Принудительный уклон: %s"),
        NpcCombatDebugCommandSystem::HasActiveDodge(npc)
            ? L("active", "активен") : L("no", "нет"));
    if (modules->runtimeForcedTime > 0.0f)
    {
        ImGui::TextColored(COL_ORANGE, L(
            "Temporary module override: 0x%X | %.2f s",
            "Временное разрешение модулей: 0x%X | %.2f с"),
            modules->runtimeForcedMask,
            modules->runtimeForcedTime);
    }

    const auto queueAction = [&](NpcDebugCombatAction action)
    {
        NpcDebugCombatActionRequest request;
        request.npc = npc;
        request.action = action;
        NpcSimulationStudio::QueueCombatAction(request);
    };
    const float halfWidth = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;
    const auto actionButton = [&](const char* label, NpcDebugCombatAction action)
    {
        if (ImGui::Button(label, ImVec2(halfWidth, 0.0f)))
            queueAction(action);
    };

    ImGui::SeparatorText(L("Forced actions", "Принудительные действия"));
    ImGui::TextWrapped("%s", L(
        "Buttons request one real gameplay action. Collision, NavMesh, visibility and weapon safety checks stay active.",
        "Кнопки запрашивают одно настоящее игровое действие. Коллизии, NavMesh, видимость и безопасность оружия остаются активны."));

    actionButton(L("Acquire nearest enemy", "Назначить ближайшего врага"),
        NpcDebugCombatAction::AcquireNearestTarget);
    ImGui::SameLine();
    actionButton(L("Face target", "Повернуться к цели"),
        NpcDebugCombatAction::FaceTarget);
    actionButton(L("Chase target", "Преследовать цель"),
        NpcDebugCombatAction::ChaseTarget);
    ImGui::SameLine();
    actionButton(L("Hold position", "Удерживать позицию"),
        NpcDebugCombatAction::HoldPosition);
    actionButton(L("Enter fire stance", "Принять огневую стойку"),
        NpcDebugCombatAction::FireStance);
    ImGui::SameLine();
    actionButton(L("Find cover", "Найти укрытие"),
        NpcDebugCombatAction::FindCover);
    actionButton(L("Dodge left", "Уклон влево"),
        NpcDebugCombatAction::DodgeLeft);
    ImGui::SameLine();
    actionButton(L("Dodge right", "Уклон вправо"),
        NpcDebugCombatAction::DodgeRight);
    actionButton(L("Peek left", "Выглянуть слева"),
        NpcDebugCombatAction::PeekLeft);
    ImGui::SameLine();
    actionButton(L("Return behind cover", "Вернуться за укрытие"),
        NpcDebugCombatAction::ReturnBehindCover);
    actionButton(L("Choose another cover", "Выбрать другое укрытие"),
        NpcDebugCombatAction::Reposition);
    ImGui::SameLine();
    actionButton(L("Cancel forced action", "Отменить действие"),
        NpcDebugCombatAction::CancelForcedAction);

    ImGui::TextColored(
        NpcSimulationStudio::lastCombatActionSucceeded ? COL_GREEN : COL_ORANGE,
        "%s: %s",
        L("Last result", "Последний результат"),
        NpcSimulationStudio::lastCombatActionResult.c_str());
}

void EditorUI::UpdateDebugOverlay(SceneManager& sceneManager)
{
    NewFrame();
    EditorDebug::RenderObjectDebugWindow(sceneManager, m_selectedEntity, m_showObjectDebugWindow);
    RenderNpcPlayDebugWindow(sceneManager, m_selectedEntity);
}

void EditorUI::UpdatePlayOverlay(SceneManager& sceneManager, bool uiCursorActive)
{
    NewFrame();
    if (NpcSimulationStudio::enabled)
    {
        // Используем существующие редакторские панели поверх живой симуляции.
        RenderSidebar(sceneManager);
        RenderDetailsPanel(sceneManager);
        const bool npcStudioRussian = NpcSimulationStudio::russianInterface;
        // Окно остаётся растягиваемым, но не может схлопнуться до ширины, при
        // которой вкладки и журнал перестают быть читаемыми.
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(500.0f, 360.0f),
            ImVec2((std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)()));
        if (ImGui::Begin(npcStudioRussian
            ? "Студия логики NPC###NPC Logic Studio"
            : "NPC Logic Studio###NPC Logic Studio"))
        {
            const auto L = [&](const char* english, const char* russian)
            {
                return NpcSimulationStudio::russianInterface ? russian : english;
            };
            const auto stateLabel = [&](NpcState state)
            {
                if (!NpcSimulationStudio::russianInterface) return NpcStateName(state);
                switch (state)
                {
                case NpcState::Idle: return "Бездействие";
                case NpcState::Wandering: return "Блуждание";
                case NpcState::MoveToPosition: return "Движение к точке";
                case NpcState::MoveToRoom: return "Движение в комнату";
                case NpcState::Following: return "Следование";
                case NpcState::MoveToRepair: return "Движение к ремонту";
                case NpcState::WaitingForDoor: return "Ожидание двери";
                case NpcState::Repairing: return "Ремонт";
                case NpcState::Stopped: return "Остановлен";
                case NpcState::Blocked: return "Заблокирован";
                case NpcState::Chasing: return "Преследование";
                case NpcState::Attacking: return "Атака";
                case NpcState::Searching: return "Поиск";
                case NpcState::Patrolling: return "Патрулирование";
                case NpcState::TakingCover: return "Движение в укрытие";
                case NpcState::InCover: return "В укрытии";
                default: return "Неизвестно";
                }
            };
            const auto commandLabel = [&](NpcCommandType command)
            {
                if (!NpcSimulationStudio::russianInterface) return NpcCommandName(command);
                switch (command)
                {
                case NpcCommandType::None: return "Автоматический Utility AI";
                case NpcCommandType::Stop: return "Стоять";
                case NpcCommandType::Wander: return "Бродить";
                case NpcCommandType::MoveToPosition: return "Идти к точке";
                case NpcCommandType::MoveToRoom: return "Идти в комнату";
                case NpcCommandType::FollowEntity: return "Следовать за объектом";
                case NpcCommandType::RepairEntity: return "Ремонтировать объект";
                default: return "Неизвестно";
                }
            };

            // Кнопки размера нужны в том числе когда нижний resize-grip ушёл за
            // экран и пользователь физически не может схватить его мышью.
            if (ImGui::Button(L("Compact width###NpcStudioCompact", "Компактно###NpcStudioCompact")))
            {
                ImVec2 size = ImGui::GetWindowSize();
                size.x = 500.0f;
                ImGui::SetWindowSize(size);
            }
            ImGui::SameLine();
            if (ImGui::Button(L("Wide view###NpcStudioWide", "Расширить окно###NpcStudioWide")))
            {
                ImVec2 size = ImGui::GetWindowSize();
                const float availableWidth = ImGui::GetMainViewport()->WorkSize.x -
                    ImGui::GetWindowPos().x - 20.0f;
                size.x = (std::max)(500.0f, (std::min)(900.0f, availableWidth));
                ImGui::SetWindowSize(size);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s", L("Interface language", "Язык интерфейса"));
            ImGui::SameLine();
            if (ImGui::RadioButton("EN###NpcStudioEnglish", !NpcSimulationStudio::russianInterface))
                NpcSimulationStudio::russianInterface = false;
            ImGui::SameLine();
            if (ImGui::RadioButton("RU###NpcStudioRussian", NpcSimulationStudio::russianInterface))
                NpcSimulationStudio::russianInterface = true;

            auto& simRegistry = sceneManager.GetECSRegistry();
            Entity playerEntity = INVALID_ENTITY;
            simRegistry.ForEach<PlayerGameTag>([&](Entity entity, PlayerGameTag&)
            {
                if (playerEntity == INVALID_ENTITY) playerEntity = entity;
            });

            // Выбор NPC общий для инспектора, симулятора урона и фильтра истории.
            // Список основан на NPCComponent, а не на squad/tactical или brain:
            // так инспектор показывает и бойца с неполным набором AI-компонентов.
            if (!simRegistry.HasComponent<NPCComponent>(NpcSimulationStudio::inspectedNpc))
                NpcSimulationStudio::inspectedNpc = INVALID_ENTITY;
            const std::string selectedNpc = NpcSimulationStudio::inspectedNpc == INVALID_ENTITY
                ? L("All NPCs", "Все NPC") : EditorEntityDebugLabel(simRegistry, NpcSimulationStudio::inspectedNpc);
            if (ImGui::BeginCombo(L("Inspected NPC###InspectedNpc", "Выбранный NPC###InspectedNpc"), selectedNpc.c_str()))
            {
                if (ImGui::Selectable(L("All NPCs###AllNpcs", "Все NPC###AllNpcs"),
                    NpcSimulationStudio::inspectedNpc == INVALID_ENTITY))
                    NpcSimulationStudio::inspectedNpc = INVALID_ENTITY;
                simRegistry.ForEach<NPCComponent>([&](Entity npc, NPCComponent&)
                {
                    const std::string label = EditorEntityDebugLabel(simRegistry, npc);
                    const bool selected = NpcSimulationStudio::inspectedNpc == npc;
                    if (ImGui::Selectable(label.c_str(), selected))
                    {
                        NpcSimulationStudio::inspectedNpc = npc;
                        SetSelection(SelectedType::Object, npc);
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                });
                ImGui::EndCombo();
            }

            if (ImGui::BeginTabBar("NpcStudioTabs"))
            {
                if (ImGui::BeginTabItem(L("Simulation###SimulationTab", "Симуляция###SimulationTab")))
                {
                    ImGui::TextWrapped("%s", L(
                        "Live simulation. Hover UI to edit; RMB outside UI to look; WASD + R/F to fly.",
                        "Живая симуляция. Наведите мышь на UI для ввода; ПКМ вне UI — обзор; WASD + R/F — полёт."));
                    ImGui::Checkbox(L("Pause simulation###PauseSimulation", "Пауза симуляции###PauseSimulation"),
                        &NpcSimulationStudio::paused);
                    ImGui::SliderFloat(L("Simulation speed###SimulationSpeed", "Скорость симуляции###SimulationSpeed"),
                        &NpcSimulationStudio::speed, 0.1f, 2.0f, "%.2fx");
                    if (ImGui::Button(L("Step 1/60 sec###StepSimulation", "Шаг 1/60 сек###StepSimulation")))
                    {
                        NpcSimulationStudio::paused = true;
                        NpcSimulationStudio::step = true;
                    }
                    simRegistry.ForEach<PlayerGameTag, TransformComponent>([&](Entity entity,
                        PlayerGameTag&, TransformComponent& transform)
                    {
                        ImGui::PushID(static_cast<int>(entity));
                        auto position = transform.position;
                        if (ImGui::DragFloat3(L("Target XYZ###TargetPosition", "Позиция цели XYZ###TargetPosition"),
                            &position.x, 0.1f))
                        {
                            NpcSimulationStudio::targetPosition = position;
                            NpcSimulationStudio::moveTarget = true;
                        }
                        if (ImGui::Button(L("Select target in inspector###SelectTarget",
                            "Выбрать цель в инспекторе###SelectTarget"))) m_selectedEntity = entity;
                        // Две закладки позволяют воспроизводить одинаковый выход
                        // из-за стены без ручного ввода координат и сброса AI.
                        for (int slot = 0; slot < 2; ++slot)
                        {
                            ImGui::PushID(slot);
                            if (ImGui::Button(slot == 0
                                ? L("Save target A###SaveTargetA", "Сохранить цель A###SaveTargetA")
                                : L("Save target B###SaveTargetB", "Сохранить цель B###SaveTargetB")))
                            {
                                NpcSimulationStudio::targetBookmarks[slot] = transform.position;
                                NpcSimulationStudio::hasBookmark[slot] = true;
                            }
                            ImGui::SameLine();
                            ImGui::BeginDisabled(!NpcSimulationStudio::hasBookmark[slot]);
                            if (ImGui::Button(L("Move target here###MoveTargetHere",
                                "Переместить цель сюда###MoveTargetHere")))
                            {
                                NpcSimulationStudio::targetPosition = NpcSimulationStudio::targetBookmarks[slot];
                                NpcSimulationStudio::moveTarget = true;
                            }
                            ImGui::EndDisabled();
                            ImGui::PopID();
                        }
                        if (simRegistry.HasComponent<PlayerComponent>(entity))
                        {
                            bool invulnerable =
                                simRegistry.HasComponent<InvulnerableComponent>(entity);
                            if (ImGui::Checkbox(L("Target God Mode###TargetGodMode",
                                "Бессмертие цели###TargetGodMode"), &invulnerable))
                            {
                                if (invulnerable &&
                                    !simRegistry.HasComponent<InvulnerableComponent>(entity))
                                {
                                    simRegistry.AddComponent<InvulnerableComponent>(
                                        entity, InvulnerableComponent{});
                                }
                                else if (!invulnerable)
                                {
                                    simRegistry.RemoveComponent<InvulnerableComponent>(entity);
                                }
                            }
                        }
                        ImGui::PopID();
                    });
                    auto& nav = NpcNavMeshSystem::GetDebugDrawSettings();
                    ImGui::SeparatorText(L("Navigation overlay", "Отладка навигации"));
                    ImGui::Checkbox(L("Enabled###NavEnabled", "Включена###NavEnabled"), &nav.enabled);
                    ImGui::Checkbox(L("NavMesh###NavMesh", "Навигационная сетка###NavMesh"), &nav.drawMesh);
                    ImGui::Checkbox(L("Paths###NavPaths", "Маршруты###NavPaths"), &nav.drawPath);
                    ImGui::Checkbox(L("Cover points###CoverPoints", "Точки укрытий###CoverPoints"), &nav.drawCoverPoints);
                    ImGui::Checkbox(L("Dynamic cover rays###DynamicCoverRays",
                        "Лучи динамического поиска укрытий###DynamicCoverRays"),
                        &nav.drawCoverQueries);
                    if (ImGui::Button(L("Rebuild navigation###RebuildNavigation",
                        "Перестроить навигацию###RebuildNavigation"))) NpcNavMeshSystem::InvalidateAll();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(L(
                    "Combat Lab###CombatLabTab",
                    "Лаборатория боя###CombatLabTab")))
                {
                    RenderNpcCombatLab(
                        simRegistry,
                        NpcSimulationStudio::inspectedNpc,
                        NpcSimulationStudio::russianInterface);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(L("Damage###DamageTab", "Урон###DamageTab")))
                {
                    static float damageAmount = 20.0f;
                    static int damageType = 0;
                    ImGui::TextWrapped("%s", L(
                        "Uses the real DamageSystem pipeline. Near miss sends threat without reducing health.",
                        "Использует настоящий DamageSystem. Близкий выстрел создаёт угрозу без потери здоровья."));
                    ImGui::DragFloat(L("Damage amount###DamageAmount", "Количество урона###DamageAmount"),
                        &damageAmount, 1.0f, 0.1f, 10000.0f, "%.1f HP");
                    const char* damageTypesEnglish[] = { "Physical", "Fire", "Electric", "Laser" };
                    const char* damageTypesRussian[] = { "Физический", "Огонь", "Электричество", "Лазер" };
                    const char** damageTypes = NpcSimulationStudio::russianInterface
                        ? damageTypesRussian : damageTypesEnglish;
                    ImGui::Combo(L("Damage type###DamageType", "Тип урона###DamageType"),
                        &damageType, damageTypes, IM_ARRAYSIZE(damageTypesEnglish));
                    const Entity damageTarget = NpcSimulationStudio::inspectedNpc;
                    ImGui::BeginDisabled(damageTarget == INVALID_ENTITY);
                    if (ImGui::Button(L("Apply hit###ApplyHit", "Нанести удар###ApplyHit"),
                        ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0.0f)))
                    {
                        NpcDebugDamageRequest request;
                        request.target = damageTarget;
                        request.source = playerEntity;
                        request.amount = damageAmount;
                        const DamageEvent::DamageType mappedTypes[] = {
                            DamageEvent::DamageType::Physical, DamageEvent::DamageType::Fire,
                            DamageEvent::DamageType::Electric, DamageEvent::DamageType::Laser };
                        request.type = mappedTypes[damageType];
                        NpcSimulationStudio::QueueDamage(request);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(L("Near miss###NearMiss", "Близкий выстрел###NearMiss"), ImVec2(-1.0f, 0.0f)))
                    {
                        NpcDebugDamageRequest request;
                        request.target = damageTarget;
                        request.source = playerEntity;
                        request.nearMiss = true;
                        NpcSimulationStudio::QueueDamage(request);
                    }
                    ImGui::EndDisabled();
                    if (damageTarget == INVALID_ENTITY)
                        ImGui::TextDisabled("%s", L(
                            "Select one NPC above to enable damage simulation.",
                            "Выберите одного NPC выше, чтобы включить симуляцию урона."));
                    else if (const auto* vitals =
                        simRegistry.GetComponent<CharacterVitalsComponent>(damageTarget))
                        ImGui::Text(L("Health: %.1f / %.1f", "Здоровье: %.1f / %.1f"),
                            vitals->health, vitals->maxHealth);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(L("Decision History###HistoryTab", "История решений###HistoryTab")))
                {
                    static bool onlySelected = true;
                    static bool showDecision = true, showMovement = true, showDamage = true;
                    static bool showPerception = true, showNavigation = true, showWarnings = true;
                    static char search[96]{};
                    ImGui::Checkbox(L("Only inspected NPC###OnlyInspected", "Только выбранный NPC###OnlyInspected"), &onlySelected);
                    ImGui::SameLine(); ImGui::Checkbox(L("Decision###FilterDecision", "Решения###FilterDecision"), &showDecision);
                    ImGui::SameLine(); ImGui::Checkbox(L("Movement###FilterMovement", "Движение###FilterMovement"), &showMovement);
                    ImGui::Checkbox(L("Damage###FilterDamage", "Урон###FilterDamage"), &showDamage);
                    ImGui::SameLine(); ImGui::Checkbox(L("Perception###FilterPerception", "Восприятие###FilterPerception"), &showPerception);
                    ImGui::SameLine(); ImGui::Checkbox(L("Navigation###FilterNavigation", "Навигация###FilterNavigation"), &showNavigation);
                    ImGui::SameLine(); ImGui::Checkbox(L("Warnings###FilterWarnings", "Предупреждения###FilterWarnings"), &showWarnings);
                    ImGui::InputTextWithHint("##history-search", L(
                        "Search title, reason or entity...", "Поиск по событию, причине или Entity..."),
                        search, IM_ARRAYSIZE(search));
                    ImGui::Checkbox(L("Freeze on decision###FreezeDecision", "Пауза при решении###FreezeDecision"),
                        &NpcSimulationStudio::freezeOnDecision);
                    ImGui::SameLine(); ImGui::Checkbox(L("damage###FreezeDamage", "уроне###FreezeDamage"),
                        &NpcSimulationStudio::freezeOnDamage);
                    ImGui::SameLine(); ImGui::Checkbox(L("warning###FreezeWarning", "предупреждении###FreezeWarning"),
                        &NpcSimulationStudio::freezeOnWarning);

                    if (ImGui::Button(L("Clear history###ClearHistory", "Очистить историю###ClearHistory")))
                        NpcSimulationStudio::ClearHistory();
                    ImGui::SameLine();
                    static std::string exportStatus;
                    if (ImGui::Button(L("Export TXT###ExportTxt", "Экспорт TXT###ExportTxt")))
                        exportStatus = NpcSimulationStudio::ExportHistory("npc_debug_history.txt", false,
                            NpcSimulationStudio::russianInterface)
                            ? L("Saved: npc_debug_history.txt", "Сохранено: npc_debug_history.txt")
                            : L("TXT export failed", "Ошибка экспорта TXT");
                    ImGui::SameLine();
                    if (ImGui::Button(L("Export JSON###ExportJson", "Экспорт JSON###ExportJson")))
                        exportStatus = NpcSimulationStudio::ExportHistory("npc_debug_history.json", true,
                            NpcSimulationStudio::russianInterface)
                            ? L("Saved: npc_debug_history.json", "Сохранено: npc_debug_history.json")
                            : L("JSON export failed", "Ошибка экспорта JSON");
                    if (!exportStatus.empty()) ImGui::TextDisabled("%s", exportStatus.c_str());

                    const auto categoryVisible = [&](NpcDebugEventCategory category)
                    {
                        switch (category)
                        {
                        case NpcDebugEventCategory::Decision: return showDecision;
                        case NpcDebugEventCategory::Movement: return showMovement;
                        case NpcDebugEventCategory::Damage: return showDamage;
                        case NpcDebugEventCategory::Perception: return showPerception;
                        case NpcDebugEventCategory::Navigation: return showNavigation;
                        case NpcDebugEventCategory::Warning: return showWarnings;
                        default: return true;
                        }
                    };
                    const auto categoryLabel = [&](NpcDebugEventCategory category)
                    {
                        switch (category)
                        {
                        case NpcDebugEventCategory::Decision: return L("decision", "решение");
                        case NpcDebugEventCategory::Movement: return L("movement", "движение");
                        case NpcDebugEventCategory::Damage: return L("damage", "урон");
                        case NpcDebugEventCategory::Perception: return L("perception", "восприятие");
                        case NpcDebugEventCategory::Navigation: return L("navigation", "навигация");
                        case NpcDebugEventCategory::Warning: return L("warning", "предупреждение");
                        default: return L("debug", "отладка");
                        }
                    };
                    if (ImGui::BeginChild("NpcDecisionHistory", ImVec2(0.0f, 330.0f), true))
                    {
                        for (const auto& event : NpcSimulationStudio::Events())
                        {
                            if (!categoryVisible(event.category)) continue;
                            if (onlySelected && NpcSimulationStudio::inspectedNpc != INVALID_ENTITY &&
                                event.npc != NpcSimulationStudio::inspectedNpc) continue;
                            const std::string searchable = event.title + " " + event.details + " " +
                                event.titleEnglish + " " + event.detailsEnglish + " " +
                                std::to_string(static_cast<unsigned>(event.npc));
                            if (!ContainsCaseInsensitive(searchable.c_str(), search)) continue;
                            const std::string& eventTitle = NpcSimulationStudio::russianInterface
                                ? event.title : event.titleEnglish;
                            const std::string& eventDetails = NpcSimulationStudio::russianInterface
                                ? event.details : event.detailsEnglish;
                            ImGui::PushID(static_cast<int>(event.sequence));
                            ImGui::Text("[%7.3f] NPC %u | %s", event.timeSeconds,
                                static_cast<unsigned>(event.npc), categoryLabel(event.category));
                            ImGui::TextWrapped("%s", eventTitle.c_str());
                            if (!eventDetails.empty())
                            {
                                // TextDisabled не переносит строку сам. Цвет и
                                // перенос задаются раздельно, чтобы причина была
                                // видна целиком даже в компактном окне.
                                ImGui::PushStyleColor(ImGuiCol_Text,
                                    ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                                ImGui::TextWrapped("%s", eventDetails.c_str());
                                ImGui::PopStyleColor();
                                if (ImGui::IsItemHovered())
                                    ImGui::SetTooltip("%s", eventDetails.c_str());
                            }
                            ImGui::Separator();
                            ImGui::PopID();
                        }
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(L("Inspector###InspectorTab", "Инспектор###InspectorTab")))
                {
                    const Entity npc = NpcSimulationStudio::inspectedNpc;
                    if (const auto* vitals =
                        simRegistry.GetComponent<CharacterVitalsComponent>(npc))
                    {
                        ImGui::Text(L("Health: %.1f / %.1f", "Здоровье: %.1f / %.1f"),
                            vitals->health, vitals->maxHealth);
                    }
                    if (const auto* brain = simRegistry.GetComponent<NpcBrainComponent>(npc))
                    {
                        ImGui::Text(L("State: %s", "Состояние: %s"), stateLabel(brain->state));
                        ImGui::Text(L("Command: %s", "Команда: %s"), commandLabel(brain->command.type));
                        ImGui::Text(L("Door route: %zu / %zu", "Маршрут через двери: %zu / %zu"),
                            brain->doorRouteIndex, brain->doorRoute.size());
                        ImGui::TextWrapped(L("Status: %s", "Статус: %s"), brain->status.c_str());
                        ImGui::TextWrapped(L("Blocked: %s", "Блокировка: %s"), brain->blockedReason.c_str());
                    }
                    else if (npc != INVALID_ENTITY)
                    {
                        ImGui::TextDisabled("%s", L(
                            "NpcBrainComponent is missing.",
                            "Отсутствует NpcBrainComponent."));
                    }
                    if (const auto* combat = simRegistry.GetComponent<NPCCombatComponent>(npc))
                    {
                        ImGui::SeparatorText(L("Combat", "Бой"));
                        ImGui::Text(L("Target: %u | visible: %s", "Цель: %u | видна: %s"),
                            static_cast<unsigned>(combat->targetEntity),
                            combat->targetVisible ? L("yes", "да") : L("no", "нет"));
                        ImGui::TextWrapped(L("Fire: %s", "Стрельба: %s"),
                            combat->rangedStatus.c_str());
                        ImGui::Text(L("Aim: %.2f / %.2f sec", "Прицеливание: %.2f / %.2f сек"),
                            combat->rangedAimTime, combat->rangedAimDelay);
                        ImGui::Text(L("Last known target: %s", "Последняя известная позиция: %s"),
                            combat->hasLastKnownPosition ? L("yes", "есть") : L("no", "нет"));
                        if (combat->hasLastKnownPosition)
                        {
                            ImGui::Text("XYZ: %.2f, %.2f, %.2f",
                                combat->lastKnownPosition.x,
                                combat->lastKnownPosition.y,
                                combat->lastKnownPosition.z);
                        }
                        const Entity weaponEntity =
                            WeaponSystem::GetEquippedWeaponEntity(simRegistry, npc);
                        if (const auto* weapon =
                            simRegistry.GetComponent<WeaponComponent>(weaponEntity))
                        {
                            ImGui::Text(L("Weapon %u | cooldown %.2f | shots %u",
                                "Оружие %u | задержка %.2f | выстрелов %u"),
                                static_cast<unsigned>(weaponEntity),
                                weapon->currentCooldown,
                                weapon->shotsFired);
                            ImGui::Text(L("Ammo: %d / %d | reserve %d",
                                "Патроны: %d / %d | запас %d"),
                                weapon->ammoInMagazine,
                                weapon->magazineSize,
                                weapon->reserveAmmo);
                            ImGui::Text(L("Reload: %s | %.2f sec remaining",
                                "Перезарядка: %s | осталось %.2f сек"),
                                weapon->isReloading ? L("active", "идёт") : L("no", "нет"),
                                weapon->reloadTimer);

                            // Runtime-выдача нужна для долгих бессмертных
                            // тренировок. Патроны попадают в резерв, поэтому
                            // пустой магазин затем проходит обычную перезарядку.
                            static int npcAmmoGrant = 30;
                            ImGui::SetNextItemWidth(110.0f);
                            ImGui::DragInt(L("Ammo grant", "Выдать патронов"),
                                &npcAmmoGrant, 1.0f, 1, 1000);
                            npcAmmoGrant = std::clamp(npcAmmoGrant, 1, 1000);
                            if (ImGui::Button(L(
                                    "Add to reserve",
                                    "Добавить в запас")))
                            {
                                WeaponSystem::GiveAmmo(
                                    simRegistry,
                                    npc,
                                    npcAmmoGrant);
                            }
                        }
                        else
                        {
                            ImGui::TextDisabled("%s", L(
                                "No equipped WeaponComponent.",
                                "Нет экипированного WeaponComponent."));
                        }
                    }
                    if (auto* tactical = simRegistry.GetComponent<NpcTacticalComponent>(npc))
                    {
                        ImGui::SeparatorText(L("Tactical decision", "Тактическое решение"));
                        const char* styleNamesEnglish[] =
                        {
                            "Auto", "Balanced", "Assault", "Defensive"
                        };
                        const char* styleNamesRussian[] =
                        {
                            "Авто", "Сбалансированная", "Штурм", "Оборона"
                        };
                        int configuredStyle =
                            static_cast<int>(tactical->selectedStyle);
                        if (ImGui::Combo(L(
                                "Tactical style###StudioTacticalStyle",
                                "Тактический стиль###StudioTacticalStyle"),
                                &configuredStyle,
                                NpcSimulationStudio::russianInterface
                                    ? styleNamesRussian
                                    : styleNamesEnglish,
                                IM_ARRAYSIZE(styleNamesEnglish)))
                        {
                            tactical->selectedStyle =
                                static_cast<NpcTacticalStyle>(configuredStyle);
                            tactical->tacticCommitTimer = 0.0f;
                        }
                        ImGui::Text(L(
                            "Configured style: %s",
                            "Заданный стиль: %s"),
                            NpcSimulationStudio::russianInterface
                                ? NpcTacticalStyleNameRu(tactical->selectedStyle)
                                : NpcTacticalStyleName(tactical->selectedStyle));
                        ImGui::Text(L(
                            "Active style: %s | commitment %.2f sec",
                            "Активный стиль: %s | фиксация %.2f сек"),
                            NpcSimulationStudio::russianInterface
                                ? NpcTacticalStyleNameRu(tactical->activeStyle)
                                : NpcTacticalStyleName(tactical->activeStyle),
                            tactical->tacticCommitTimer);
                        ImGui::TextWrapped(L(
                            "Tactic reason: %s",
                            "Причина тактики: %s"),
                            tactical->tacticReason.c_str());
                        ImGui::TextWrapped(L("Order: %s", "Приказ: %s"), tactical->status.c_str());
                        ImGui::TextWrapped(L("Reason: %s", "Причина: %s"), tactical->lastTacticalTransition.c_str());
                        ImGui::Text(L("Decision %.2f | reposition %.2f | stuck %.2f",
                            "Решение %.2f | смена позиции %.2f | без движения %.2f"),
                            tactical->decisionTimer, tactical->repositionCooldown, tactical->noProgressTime);
                        ImGui::Text(L("Under fire: %.2f", "Под огнём: %.2f"),
                            tactical->underFireTime);
                        ImGui::Text(L(
                            "Recent real hits: %d | window %.2f",
                            "Недавние реальные попадания: %d | окно %.2f"),
                            tactical->recentDamageHits,
                            tactical->recentDamageWindow);
                        ImGui::Text(L(
                            "Cover commitment: %.2f sec",
                            "Обязательство удерживать укрытие: %.2f сек"),
                            tactical->coverHoldTimer);
                        ImGui::Text(L(
                            "Incoming direction: %s | turn %.2f",
                            "Направление огня: %s | поворот %.2f"),
                            tactical->hasIncomingFireDirection
                                ? L("known", "известно")
                                : L("unknown", "неизвестно"),
                            tactical->incomingFireTurnTimer);
                        ImGui::Text(L(
                            "Failed peeks: %d | cover change: %s",
                            "Неудачные выглядывания: %d | смена укрытия: %s"),
                            tactical->failedPeekCount,
                            tactical->coverChangeRequested
                                ? L("requested", "запрошена")
                                : L("no", "нет"));
                        ImGui::Text(L(
                            "Moving burst: %d left | pause %.2f",
                            "Очередь на ходу: осталось %d | пауза %.2f"),
                            tactical->movingBurstShotsRemaining,
                            tactical->movingBurstPauseTimer);
                        const bool defensiveCoverRequest =
                            tactical->activeStyle == NpcTacticalStyle::Defensive;
                        const bool assaultStyle =
                            tactical->activeStyle == NpcTacticalStyle::Assault;
                        ImGui::TextWrapped("%s", defensiveCoverRequest
                            ? L("Cover request is active: the defensive style proactively seeks protection.",
                                "Запрос укрытия активен: оборонительный стиль заранее ищет защиту.")
                            : assaultStyle
                                ? L("Assault ignores suppression and one isolated hit; repeated real hits or critical health request cover.",
                                    "Штурм игнорирует подавление и одиночное попадание; укрытие запрашивают серия реальных попаданий или критическое здоровье.")
                            : tactical->underFireTime > 0.0f
                                ? L("Cover request is active because this NPC was recently hit or threatened.",
                                    "Запрос укрытия активен: NPC недавно получил попадание или сигнал угрозы.")
                                : L("Cover request is inactive until this style receives damage or a near miss.",
                                    "Запрос укрытия неактивен, пока этот стиль не получит урон или «Близкий выстрел»."));
                        if (tactical->dynamicCoverActive)
                        {
                            ImGui::TextWrapped("%s", L(
                                "Cover: dynamically discovered (no authored Entity)",
                                "Укрытие: найдено динамически (без авторской Entity)"));
                            ImGui::Text(L("Cover XYZ: %.2f, %.2f, %.2f",
                                "Позиция укрытия: %.2f, %.2f, %.2f"),
                                tactical->coverPosition.x,
                                tactical->coverPosition.y,
                                tactical->coverPosition.z);
                        }
                        else if (tactical->reservedCover == INVALID_ENTITY)
                        {
                            ImGui::Text(L("Reserved cover: none", "Зарезервированное укрытие: нет"));
                        }
                        else
                        {
                            ImGui::Text(L("Reserved cover: %u", "Зарезервированное укрытие: %u"),
                                static_cast<unsigned>(tactical->reservedCover));
                            ImGui::Text(L("Cover XYZ: %.2f, %.2f, %.2f",
                                "Позиция укрытия: %.2f, %.2f, %.2f"),
                                tactical->coverPosition.x,
                                tactical->coverPosition.y,
                                tactical->coverPosition.z);
                            ImGui::Text(L("Phase: %s | timer %.2f",
                                "Фаза: %s | таймер %.2f"),
                                tactical->movingToPeek
                                    ? L("moving to peek", "выход к краю")
                                    : tactical->exposed
                                        ? L("exposed", "открыт")
                                        : L("protected", "защищён"),
                                tactical->phaseTimer);
                        }
                        ImGui::Text(L("Rejected: protection %d | sight %d | path %d",
                            "Отклонено: защита %d | обзор %d | путь %d"),
                            tactical->coverRejectedProtection,
                            tactical->coverRejectedSight,
                            tactical->coverRejectedPath);
                        if (tactical->reservedCover != INVALID_ENTITY ||
                            tactical->dynamicCoverActive ||
                            tactical->hasFlankTarget || tactical->movingToPeek)
                        {
                            const auto goal = tactical->hasFlankTarget
                                ? tactical->flankTarget : tactical->moveTarget;
                            ImGui::Text(L("Goal %.2f, %.2f, %.2f (%s)",
                                "Цель %.2f, %.2f, %.2f (%s)"),
                                goal.x, goal.y, goal.z, tactical->hasFlankTarget
                                    ? L("flank", "фланг") : L("move", "движение"));
                        }
                    }
                    if (npc == INVALID_ENTITY) ImGui::TextDisabled("%s", L(
                        "Select an NPC above.", "Выберите NPC выше."));
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            if (ImGui::Button(L("Exit Logic Studio###ExitLogicStudio",
                "Выйти из NPC Studio###ExitLogicStudio"))) NpcSimulationStudio::enabled = false;
            ImGui::TextWrapped("%s", L(
                "Edits affect the running scene. This is not an isolated scene copy.",
                "Изменения влияют на запущенную сцену. Это не отдельная копия сцены."));
        }
        ImGui::End();
    }
    // F4 не конфликтует с техническим HUD на F3.
    static bool showCombatMonitor = false;
    if (ImGui::IsKeyPressed(ImGuiKey_F4, false)) showCombatMonitor = !showCombatMonitor;
    if (showCombatMonitor)
    {
        if (ImGui::Begin("NPC Combat Monitor [F4]", &showCombatMonitor))
        {
            ImGui::TextDisabled("F4: toggle | UI hover / RMB free-look | live runtime state");
            auto& registry = sceneManager.GetECSRegistry();
            // Start from NPCComponent so the monitor never hides an NPC merely
            // because one of its combat setup components is missing.
            registry.ForEach<NPCComponent>([&](Entity npc, NPCComponent&) {
                auto* combat = registry.GetComponent<NPCCombatComponent>(npc);
                auto* brain = registry.GetComponent<NpcBrainComponent>(npc);
                auto* vitals = registry.GetComponent<CharacterVitalsComponent>(npc);
                ImGui::Separator();
                ImGui::Text("NPC %u | %s | %.1f / %.1f HP | %s",
                    static_cast<unsigned>(npc),
                    brain ? NpcStateName(brain->state) : "missing brain",
                    vitals ? vitals->health : 0.0f,
                    vitals ? vitals->maxHealth : 0.0f,
                    registry.HasComponent<ActiveComponent>(npc) ? "active" : "inactive");
                if (combat)
                {
                    ImGui::Text("Visible: %s | target: %u | forget timer: %.1f",
                        combat->targetVisible ? "yes" : "no",
                        static_cast<unsigned>(combat->targetEntity),
                        combat->timeSinceTargetSeen);
                    ImGui::TextWrapped("Fire: %s", combat->rangedStatus.c_str());
                }
                else
                {
                    ImGui::TextDisabled("NPCCombatComponent is missing");
                }
                const auto nav = NpcNavMeshSystem::GetDebugInfo(npc);
                ImGui::TextWrapped("NavMesh: %s | polygons: %d | path points: %d",
                    nav.status.c_str(), nav.polygonCount, nav.waypointCount);
                if (auto* t = registry.GetComponent<NpcTacticalComponent>(npc))
                {
                    ImGui::Text("Individual tactics | cover %u | under fire %.1fs",
                        static_cast<unsigned>(t->reservedCover), t->underFireTime);
                    ImGui::TextWrapped("Order: %s", t->status.c_str());
                    ImGui::TextWrapped("Last transition: %s", t->lastTacticalTransition.c_str());
                    ImGui::Text("Firing position hold: %.1fs", t->firingPositionHold);
                    ImGui::Text("Cover rejected: protection %d | sight %d | path %d",
                        t->coverRejectedProtection, t->coverRejectedSight, t->coverRejectedPath);
                    ImGui::Text("Moving aim: %.2fs | no suitable cover: %s",
                        t->movingAimTime, t->noSuitableCover ? "yes" : "no");
                    ImGui::Text("Search failures: %d/10 | retry: %.1fs | en route: %s",
                        t->searchRouteFailures, t->searchRetryTime, t->searchingSector ? "yes" : "no");
                    ImGui::Text("Rejected: navigation %d | spacing %d | near/visited %d",
                        t->searchRejectedNavigation, t->searchRejectedSpacing, t->searchRejectedNear);
                    ImGui::Text("Reaction cooldown: %.1fs", t->damageReactionCooldown);
                    ImGui::Text("Route: %.1fs | reposition: %.1fs | stuck: %.1fs",
                        t->flankCommitTime, t->repositionCooldown, t->noProgressTime);
                    const auto goal = t->hasFlankTarget ? t->flankTarget : t->moveTarget;
                    ImGui::Text("Goal: %.1f %.1f %.1f | cover: %u", goal.x, goal.y, goal.z,
                        static_cast<unsigned>(t->reservedCover));
                }
            });
        }
        ImGui::End();
    }
    EditorDebug::RenderObjectDebugWindow(
        sceneManager, m_selectedEntity, m_showObjectDebugWindow);
    RenderNpcPlayDebugWindow(sceneManager, m_selectedEntity);
    RenderWeaponGripPlayWindow(sceneManager, uiCursorActive);
}

void EditorUI::CloseSocketStudio(SceneManager& sceneManager)
{
    m_socketStudioOpen = false;
    m_socketStudioClipPreview = false;
    m_socketStudioPreviewGameCamera = false;
    m_socketStudioUiCursorActive = false;
    m_animationStudioControllersActive = false;
    m_animationStudioTarget = INVALID_ENTITY;
    if (m_cameraController)
    {
        m_cameraController->SetSocketStudioEnabled(false);
        m_cameraController->SetSocketStudioFreeFly(false);
    }
    auto& registry = sceneManager.GetECSRegistry();
    registry.ForEach<AnimationComponent>([&](Entity, AnimationComponent& animation)
    {
        animation.socketPreviewEnabled = false;
        animation.socketPreviewPunch = false;
        animation.socketPreviewApplyPunchIK = false;
        animation.socketPreviewFirstPersonFromWorld = false;
    });
    registry.ForEach<FirstPersonViewComponent>([&](Entity entity, FirstPersonViewComponent& view)
    {
        view.studioFreezeCameraAnchor = false;
        view.studioCameraAnchorCaptured = false;
        if (auto* animation = registry.GetComponent<AnimationComponent>(entity))
            animation->socketPreviewEnabled = false;
    });
}

void EditorUI::RenderWeaponGripPlayWindow(
    SceneManager& sceneManager, bool uiCursorActive)
{
    auto& registry = sceneManager.GetECSRegistry();
    Entity player = INVALID_ENTITY;
    registry.ForEach<PlayerGameTag>([&](Entity candidate, PlayerGameTag&)
    {
        if (player == INVALID_ENTITY)
            player = candidate;
    });
    // Normal gameplay still uses the explicit cursor mode. NPC Studio and
    // playerless observer mode route input automatically, so their authoring
    // tools must not switch off merely because the pointer left a window.
    const bool authoringInputActive = uiCursorActive ||
        NpcSimulationStudio::enabled || player == INVALID_ENTITY;
    m_socketStudioUiCursorActive = authoringInputActive;
    if (player != INVALID_ENTITY &&
        !registry.HasComponent<PlayCameraComponent>(player))
        registry.AddComponent<PlayCameraComponent>(player, PlayCameraComponent{});
    auto* playCamera = registry.GetComponent<PlayCameraComponent>(player);

    const auto isStudioCharacter = [&](Entity entity)
    {
        if (entity == INVALID_ENTITY || !registry.IsAlive(entity)) return false;
        if (!registry.HasComponent<PlayerGameTag>(entity) &&
            !registry.HasComponent<NPCComponent>(entity)) return false;
        const auto* candidateAnimation =
            registry.GetComponent<AnimationComponent>(entity);
        const auto* candidateMesh =
            registry.GetComponent<MeshRendererComponent>(entity);
        return candidateAnimation && candidateMesh && candidateMesh->model &&
            candidateMesh->model->HasAnimations();
    };

    if (!m_socketStudioOpen && isStudioCharacter(m_selectedEntity))
        m_animationStudioTarget = m_selectedEntity;
    else if (!isStudioCharacter(m_animationStudioTarget))
        m_animationStudioTarget = isStudioCharacter(m_selectedEntity)
            ? m_selectedEntity : player;

    // The small launcher leaves the actual rendered scene free for inspection.
    ImGui::SetNextWindowPos(ImVec2(18.0f, 125.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(280.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Animation Tools"))
    {
        if (ImGui::Checkbox(NpcSimulationStudio::russianInterface
            ? "Студия логики NPC###NpcLogicStudioToggle"
            : "NPC Logic Studio###NpcLogicStudioToggle",
            &NpcSimulationStudio::enabled) && NpcSimulationStudio::enabled)
        {
            CloseSocketStudio(sceneManager);
            NpcSimulationStudio::paused = true;
            NpcSimulationStudio::step = false;
            // Новая сессия начинается с чистого baseline: старые Entity и
            // переходы предыдущего запуска не смешиваются с текущей сценой.
            NpcSimulationStudio::ClearHistory();
            auto& nav = NpcNavMeshSystem::GetDebugDrawSettings();
            nav.enabled = nav.drawPath = nav.drawMesh = nav.drawCoverPoints = nav.allNpcs = true;
            // В Studio сразу показываем и новый поиск, чтобы первый тест не
            // требовал искать отдельный переключатель в Debug-панели.
            nav.drawCoverQueries = true;
        }
        // Переключатель доступен в игре без поиска сущности игрока в инспекторе.
        registry.ForEach<PlayerComponent>([&registry](Entity entity, PlayerComponent&) {
            ImGui::PushID(static_cast<int>(entity));
            bool invulnerable =
                registry.HasComponent<InvulnerableComponent>(entity);
            if (ImGui::Checkbox("God Mode", &invulnerable))
            {
                if (invulnerable)
                {
                    if (auto* vitals =
                        registry.GetComponent<CharacterVitalsComponent>(entity))
                        vitals->health = vitals->maxHealth;
                    if (!registry.HasComponent<InvulnerableComponent>(entity))
                        registry.AddComponent<InvulnerableComponent>(
                            entity, InvulnerableComponent{});
                }
                else
                {
                    registry.RemoveComponent<InvulnerableComponent>(entity);
                }
            }
            ImGui::PopID();
        });
        const std::string currentTarget = isStudioCharacter(m_animationStudioTarget)
            ? EditorEntityDebugLabel(registry, m_animationStudioTarget)
            : std::string("No animated character");
        if (ImGui::BeginCombo("Character", currentTarget.c_str()))
        {
            const auto drawCandidate = [&](Entity candidate, const char* kind)
            {
                if (!isStudioCharacter(candidate)) return;
                const std::string label = std::string(kind) + ": " +
                    EditorEntityDebugLabel(registry, candidate);
                if (ImGui::Selectable(label.c_str(),
                        candidate == m_animationStudioTarget))
                {
                    registry.ForEach<AnimationComponent>(
                        [](Entity, AnimationComponent& preview)
                        {
                            preview.socketPreviewEnabled = false;
                            preview.socketPreviewPunch = false;
                            preview.socketPreviewApplyPunchIK = false;
                            preview.socketPreviewFirstPersonFromWorld = false;
                        });
                    m_animationStudioTarget = candidate;
                    m_animationStudioControllerView = 0;
                    m_animationStudioKeyLayer = 0;
                    m_animationStudioBone.clear();
                    m_socketStudioClipPreview = false;
                    m_socketStudioPreviewGameCamera = false;
                    if (auto* selectedAnimation =
                            registry.GetComponent<AnimationComponent>(candidate))
                        m_animationStudioProfileName =
                            selectedAnimation->profileName.empty()
                            ? (registry.HasComponent<NPCComponent>(candidate)
                                ? "NPC" : "Kevin")
                            : selectedAnimation->profileName;
                    if (m_cameraController)
                        m_cameraController->SetPlayViewMode(
                            CameraController::PlayViewMode::ThirdPerson);
                }
            };
            drawCandidate(player, "Player");
            registry.ForEach<NPCComponent>(
                [&](Entity candidate, NPCComponent&) { drawCandidate(candidate, "NPC"); });
            ImGui::EndCombo();
        }
        if (ImGui::Button(m_socketStudioOpen ? "Close Animation Studio" : "Open Animation Studio",
                ImVec2(-1.0f, 0.0f)))
        {
            const bool openingStudio = !m_socketStudioOpen;
            m_socketStudioOpen = !m_socketStudioOpen;
            if (openingStudio)
            {
                if (auto* selectedAnimation = registry.GetComponent<AnimationComponent>(
                        m_animationStudioTarget))
                    m_animationStudioProfileName =
                        !selectedAnimation->profileName.empty()
                        ? selectedAnimation->profileName
                        : (registry.HasComponent<NPCComponent>(m_animationStudioTarget)
                            ? "NPC" : "Kevin");
            }
            else
            {
                m_socketStudioClipPreview = false;
                m_socketStudioPreviewGameCamera = false;
            }
        }
        ImGui::TextDisabled("Select an NPC here or in the outliner before opening.");
        ImGui::TextDisabled("UI under cursor | RMB outside UI: look | WASD + R/F: fly");
    }
    ImGui::End();

    const Entity studioTarget = isStudioCharacter(m_animationStudioTarget)
        ? m_animationStudioTarget : player;
    const bool npcTarget = registry.HasComponent<NPCComponent>(studioTarget) &&
        !registry.HasComponent<PlayerGameTag>(studioTarget);
    const Entity weaponEntity =
        WeaponSystem::GetEquippedWeaponEntity(registry, studioTarget);
    auto* weapon = registry.GetComponent<WeaponComponent>(weaponEntity);
    auto* animation = registry.GetComponent<AnimationComponent>(studioTarget);
    auto* playerData = registry.GetComponent<PlayerComponent>(studioTarget);
    const auto* mesh = registry.GetComponent<MeshRendererComponent>(studioTarget);
    const AssimpModelClass* model = mesh ? mesh->model : nullptr;
    FirstPersonViewComponent* armsView = nullptr;
    Entity armsEntity = INVALID_ENTITY;
    if (!npcTarget)
        registry.ForEach<FirstPersonViewComponent>(
            [&](Entity entity, FirstPersonViewComponent& view)
            {
                if (view.owner == studioTarget && view.followCamera)
                {
                    armsView = &view;
                    armsEntity = entity;
                }
            });
    auto* armsAnimation = registry.GetComponent<AnimationComponent>(armsEntity);
    const auto* armsMesh = registry.GetComponent<MeshRendererComponent>(armsEntity);
    const AssimpModelClass* armsModel = armsMesh ? armsMesh->model : nullptr;

    // NPC authoring is deliberately world/third-person only.
    if (npcTarget && m_cameraController)
    {
        m_animationStudioControllerView = 0;
        m_socketStudioPreviewGameCamera = false;
        m_cameraController->SetPlayViewMode(
            CameraController::PlayViewMode::ThirdPerson);
    }

    if (m_cameraController)
    {
        m_cameraController->SetSocketStudioEnabled(
            m_socketStudioOpen && !m_socketStudioPreviewGameCamera &&
            !m_cameraController->IsSocketStudioFreeFly());
        if (!m_socketStudioOpen && !NpcSimulationStudio::enabled &&
            player != INVALID_ENTITY)
            m_cameraController->SetSocketStudioFreeFly(false);
    }
    // Preview is an authoring action, never a persistent gameplay layer.
    const bool previewActive = m_socketStudioOpen && authoringInputActive &&
        (m_socketStudioClipPreview || (animation && animation->socketPreviewPunch));
    if (animation)
    {
        animation->socketPreviewEnabled = previewActive;
        // Individual tabs opt into this below. Clearing it here prevents a
        // stale FP-source mode after switching back to Controls or Combat.
        animation->socketPreviewFirstPersonFromWorld = false;
        animation->socketPreviewApplyPunchIK = false;
    }
    if (armsAnimation)
        armsAnimation->socketPreviewEnabled = previewActive &&
            m_cameraController && m_cameraController->IsFirstPerson();
    if (armsView)
        armsView->studioFreezeCameraAnchor = m_socketStudioOpen &&
            m_cameraController && m_cameraController->IsFirstPerson() &&
            m_cameraController->IsSocketStudioFreeFly() &&
            m_socketStudioFreezeFirstPersonArms;
    if (!m_socketStudioOpen)
    {
        m_animationStudioControllersActive = false;
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    const float panelWidth = std::clamp(
        io.DisplaySize.x * 0.46f, 620.0f, 820.0f);
    const float panelTop = (std::min)(90.0f, io.DisplaySize.y * 0.12f);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - panelWidth, panelTop), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, io.DisplaySize.y - panelTop), ImGuiCond_Always);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
    if (!ImGui::Begin("Animation Studio", &m_socketStudioOpen, flags))
    {
        ImGui::End();
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, COL_HEADER);
    ImGui::BeginChild("##AnimationStudioHeader", ImVec2(0.0f, 72.0f), true);
    ImGui::TextColored(COL_ACCENT, "ANIMATION STUDIO");
    ImGui::SameLine();
    ImGui::TextDisabled(npcTarget
        ? "  NPC pose asset editor (3P only)" : "  Player pose asset editor");
    ImGui::Text("Character: %s",
        EditorEntityDebugLabel(registry, studioTarget).c_str());
    ImGui::TextDisabled("Select a view and clip, adjust a control in the viewport, then save the preset.");
    ImGui::EndChild();
    ImGui::PopStyleColor();
    const bool controllersWereActive = m_animationStudioControllersActive;
    if (m_animationStudioControllersActive)
    {
        ImGui::TextColored(m_socketStudioClipPreview ? COL_GREEN : COL_TEXT_DIM,
            m_socketStudioClipPreview
                ? "LIVE PREVIEW  |  Move a control to create a key at the playhead"
                : "LIVE PREVIEW OFF");
    }
    else if (m_socketStudioClipPreview ||
        animation && animation->socketPreviewPunch)
    {
        if (ImGui::Button("Stop animation preview"))
        {
            m_socketStudioClipPreview = false;
            if (animation) animation->socketPreviewPunch = false;
        }
        ImGui::TextDisabled("Preview is active while the studio owns authoring input.");
    }
    if (!authoringInputActive)
        ImGui::TextColored(COL_RED, "Press F2 to release the mouse for editing.");
    if (!animation || !model)
    {
        ImGui::TextColored(COL_RED,
            "Load an animated character model first.");
        ImGui::End();
        return;
    }

    ImGui::SetNextItemWidth(150.0f);
    InputStringValue("Preset", m_animationStudioProfileName);
    const std::string quickAnimationPath =
        AnimationStudioPath(m_animationStudioProfileName);
    ImGui::SameLine();
    ImGui::BeginDisabled(quickAnimationPath.empty());
    if (ImGui::Button("Save Animation", ImVec2(130.0f, 0.0f)))
    {
        const bool saved = SaveAnimationStudio(quickAnimationPath,
            *animation, armsAnimation, playerData, npcTarget);
        if (saved)
        {
            animation->profileName = m_animationStudioProfileName;
            if (armsAnimation) armsAnimation->profileName = m_animationStudioProfileName;
        }
        m_weaponProfileStatus = saved ? "Saved: " + quickAnimationPath
            : "Could not save animation preset.";
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload", ImVec2(90.0f, 0.0f)))
    {
        const bool loaded = LoadAnimationStudio(quickAnimationPath,
            *animation, armsAnimation, playerData);
        if (loaded)
        {
            animation->profileName = m_animationStudioProfileName;
            if (armsAnimation) armsAnimation->profileName = m_animationStudioProfileName;
        }
        m_weaponProfileStatus = loaded ? "Loaded: " + quickAnimationPath
            : "Could not load animation preset.";
    }
    ImGui::EndDisabled();
    if (!m_weaponProfileStatus.empty())
        ImGui::TextColored(COL_TEXT_DIM, "%s", m_weaponProfileStatus.c_str());
    ImGui::Separator();
    m_animationStudioControllersActive = false;

    if (ImGui::BeginTabBar("##SocketStudioTabs"))
    {
        if (ImGui::BeginTabItem("Controls"))
        {
            m_animationStudioControllersActive = true;
            animation->socketPreviewPunch = false;
            if (!controllersWereActive)
                m_socketStudioClipPreview = true;

            if (m_animationStudioControllerView == 1 &&
                (!armsAnimation || !armsModel))
            {
                m_animationStudioControllerView = 0;
                m_animationStudioKeyLayer = 0;
                m_animationStudioBone.clear();
                if (m_cameraController)
                    m_cameraController->SetPlayViewMode(
                        CameraController::PlayViewMode::ThirdPerson);
            }

            ImGui::SeparatorText("Preview Context");
            ImGui::Checkbox("Live preview", &m_socketStudioClipPreview);
            ImGui::SameLine();
            if (ImGui::RadioButton("Third Person", m_animationStudioControllerView == 0))
            {
                m_animationStudioControllerView = 0;
                m_animationStudioKeyLayer = 0;
                if (m_cameraController)
                    m_cameraController->SetPlayViewMode(
                        CameraController::PlayViewMode::ThirdPerson);
                m_animationStudioBone.clear();
            }
            ImGui::SameLine();
            if (!npcTarget)
            {
                ImGui::BeginDisabled(!armsAnimation || !armsModel);
                if (ImGui::RadioButton("First Person", m_animationStudioControllerView == 1))
                {
                    m_animationStudioControllerView = 1;
                    m_animationStudioKeyLayer = 0;
                    if (m_cameraController)
                        m_cameraController->SetPlayViewMode(
                            CameraController::PlayViewMode::FirstPerson);
                    m_animationStudioBone.clear();
                }
                ImGui::EndDisabled();
            }
            ImGui::SameLine();
            ImGui::TextColored(m_animationStudioControllerView == 1
                    ? COL_GREEN : COL_ACCENT,
                m_animationStudioControllerView == 1
                    ? "  1P keys are isolated per clip"
                    : npcTarget ? "  NPC world pose (no FP)"
                    : "  World character pose");

            const bool editFirstPerson = m_animationStudioControllerView == 1 &&
                armsAnimation && armsModel;
            AnimationComponent* controlAnimation = editFirstPerson
                ? armsAnimation : animation;
            const AssimpModelClass* controlModel = editFirstPerson
                ? armsModel : model;
            if (controlAnimation->socketPreviewClip < 0 ||
                !controlModel->GetAnimation(controlAnimation->socketPreviewClip))
                controlAnimation->socketPreviewClip = 0;
            const auto* baseClip = controlModel->GetAnimation(
                controlAnimation->socketPreviewClip);
            ImGui::SetNextItemWidth(360.0f);
            if (ImGui::BeginCombo("Base clip", baseClip
                    ? baseClip->name.c_str() : "Select animation clip"))
            {
                for (size_t i = 0; i < controlModel->GetAnimationCount(); ++i)
                {
                    const auto* candidate = controlModel->GetAnimation(
                        static_cast<int>(i));
                    if (!candidate) continue;
                    if (ImGui::Selectable(candidate->name.c_str(),
                            controlAnimation->socketPreviewClip == static_cast<int>(i)))
                    {
                        controlAnimation->socketPreviewClip = static_cast<int>(i);
                        controlAnimation->socketPreviewTime = 0.0f;
                        controlAnimation->currentTime = 0.0f;
                        controlAnimation->socketPreviewPlaying = false;
                        m_animationStudioBone.clear();
                        baseClip = candidate;
                    }
                }
                ImGui::EndCombo();
            }

            if (ImGui::Checkbox("Preview upper-body layer",
                    &controlAnimation->socketPreviewBlendEnabled))
            {
                controlAnimation->socketPreviewPlaying = false;
                if (controlAnimation->socketPreviewBlendEnabled &&
                    !controlModel->GetAnimation(
                        controlAnimation->socketPreviewUpperClip))
                    controlAnimation->socketPreviewUpperClip =
                        controlAnimation->socketPreviewClip;
                if (!controlAnimation->socketPreviewBlendEnabled)
                    m_animationStudioKeyLayer = 0;
                m_animationStudioBone.clear();
            }
            const AssimpModelClass::AnimationClip* upperClip = nullptr;
            if (controlAnimation->socketPreviewBlendEnabled)
            {
                upperClip = controlModel->GetAnimation(
                    controlAnimation->socketPreviewUpperClip);
                ImGui::SetNextItemWidth(360.0f);
                if (ImGui::BeginCombo("Upper-body clip", upperClip
                        ? upperClip->name.c_str() : "Select upper-body clip"))
                {
                    for (size_t i = 0; i < controlModel->GetAnimationCount(); ++i)
                    {
                        const auto* candidate = controlModel->GetAnimation(
                            static_cast<int>(i));
                        if (!candidate) continue;
                        if (ImGui::Selectable(candidate->name.c_str(),
                                controlAnimation->socketPreviewUpperClip ==
                                    static_cast<int>(i)))
                        {
                            controlAnimation->socketPreviewUpperClip =
                                static_cast<int>(i);
                            controlAnimation->socketPreviewUpperTime = 0.0f;
                            controlAnimation->upperBodyTime = 0.0f;
                            controlAnimation->socketPreviewPlaying = false;
                            m_animationStudioBone.clear();
                            upperClip = candidate;
                        }
                    }
                    ImGui::EndCombo();
                }
                if (ImGui::BeginCombo("Upper-body root",
                        controlAnimation->upperBodyRoot.c_str()))
                {
                    std::vector<std::string> roots;
                    roots.reserve(controlModel->m_boneInfoMap.size());
                    for (const auto& [name, info] : controlModel->m_boneInfoMap)
                        roots.push_back(name);
                    std::sort(roots.begin(), roots.end());
                    for (const auto& name : roots)
                        if (ImGui::Selectable(name.c_str(),
                                name == controlAnimation->upperBodyRoot))
                            controlAnimation->upperBodyRoot = name;
                    ImGui::EndCombo();
                }
                ImGui::Checkbox("Sync layer phase",
                    &controlAnimation->socketPreviewSyncLayerPhase);
                ImGui::TextDisabled("Final pose: Base below the root + Upper Body from the root down.");
                ImGui::Text("Write keys to:");
                ImGui::SameLine();
                ImGui::RadioButton("Base", &m_animationStudioKeyLayer, 0);
                ImGui::SameLine();
                ImGui::BeginDisabled(!upperClip);
                ImGui::RadioButton("Upper Body", &m_animationStudioKeyLayer, 1);
                ImGui::EndDisabled();
            }
            else
                m_animationStudioKeyLayer = 0;

            const bool editUpperLayer =
                controlAnimation->socketPreviewBlendEnabled &&
                m_animationStudioKeyLayer == 1 && upperClip;
            const auto* controlClip = editUpperLayer ? upperClip : baseClip;

            const int runtimeActionClipIndex =
                controlAnimation->socketPreviewBlendEnabled && upperClip
                ? controlAnimation->socketPreviewUpperClip
                : controlAnimation->socketPreviewClip;
            int runtimePunchVariant = -1;
            for (int index = 0;
                index < static_cast<int>(animation->punchVariants.size()); ++index)
            {
                const auto& option = animation->punchVariants[index];
                std::string clipName = editFirstPerson
                    ? option.firstPersonClip : option.worldClip;
                if (clipName.empty()) clipName = option.worldClip;
                if (!clipName.empty() &&
                    controlModel->FindAnimationIndexBySemanticName(clipName) ==
                        runtimeActionClipIndex)
                {
                    runtimePunchVariant = index;
                    break;
                }
            }
            ImGui::SeparatorText("Final runtime pose");
            ImGui::Checkbox("Apply full runtime IK stack",
                &m_animationStudioFullRuntimePreview);
            animation->socketPreviewApplyIK =
                m_animationStudioFullRuntimePreview;
            animation->socketPreviewApplyPunchIK =
                m_animationStudioFullRuntimePreview && runtimePunchVariant >= 0;
            if (runtimePunchVariant >= 0)
            {
                animation->socketPreviewPunchVariant = runtimePunchVariant;
                ImGui::TextColored(COL_GREEN,
                    "Punch IK: %s", animation->punchVariants[
                        runtimePunchVariant].name.c_str());
                ImGui::Checkbox("Runtime aim follows camera",
                    &animation->socketPreviewPunchUseCameraAim);
                if (!animation->socketPreviewPunchUseCameraAim)
                    ImGui::SliderFloat("Runtime fixed aim distance (m)",
                        &animation->socketPreviewPunchAimDistance,
                        0.1f, 5.0f, "%.2f");
                ImGui::Text("Final fist aim error: %.3f m",
                    animation->socketPreviewPunchAimError);
            }
            else
                ImGui::TextDisabled(
                    "Punch IK is inactive: the action clip is not a configured punch variant.");
            ImGui::TextDisabled(
                "Order: blend -> pose keys/arm controls -> weapon IK -> punch aim/wrist IK.");
            if (controlClip)
            {
                const float rate = controlClip->ticksPerSecond > 0.0f
                    ? controlClip->ticksPerSecond : 24.0f;
                const float duration = (std::max)(0.001f,
                    controlClip->duration / rate);
                const float previewEnd = AnimationClipPreviewEnd(duration);
                float& time = editUpperLayer
                    ? controlAnimation->socketPreviewUpperTime
                    : controlAnimation->socketPreviewTime;
                time = std::clamp(time, 0.0f, previewEnd);
                const auto clipDurationSeconds = [](
                    const AssimpModelClass::AnimationClip* clip)
                {
                    if (!clip) return 0.0f;
                    const float clipRate = clip->ticksPerSecond > 0.0f
                        ? clip->ticksPerSecond : 24.0f;
                    return clip->duration / clipRate;
                };
                const auto syncPreviewTimes = [&]()
                {
                    if (controlAnimation->socketPreviewBlendEnabled &&
                        controlAnimation->socketPreviewSyncLayerPhase &&
                        baseClip && upperClip)
                    {
                        const float baseDuration = (std::max)(0.001f,
                            clipDurationSeconds(baseClip));
                        const float upperDuration = (std::max)(0.001f,
                            clipDurationSeconds(upperClip));
                        if (editUpperLayer)
                            controlAnimation->socketPreviewTime =
                                std::clamp(time / upperDuration, 0.0f, 0.999999f) *
                                baseDuration;
                        else
                            controlAnimation->socketPreviewUpperTime =
                                std::clamp(time / baseDuration, 0.0f, 0.999999f) *
                                upperDuration;
                    }
                    controlAnimation->currentTime =
                        controlAnimation->socketPreviewTime;
                    controlAnimation->upperBodyTime =
                        controlAnimation->socketPreviewUpperTime;
                };
                syncPreviewTimes();
                if (ImGui::Button("|<"))
                {
                    time = 0.0f;
                    syncPreviewTimes();
                    controlAnimation->socketPreviewPlaying = false;
                }
                ImGui::SameLine();
                if (ImGui::Button("< Frame"))
                {
                    time = (std::max)(0.0f, time - 1.0f / 30.0f);
                    syncPreviewTimes();
                    controlAnimation->socketPreviewPlaying = false;
                }
                ImGui::SameLine();
                if (ImGui::Button(controlAnimation->socketPreviewPlaying
                        ? "Pause" : "Play"))
                    controlAnimation->socketPreviewPlaying =
                        !controlAnimation->socketPreviewPlaying;
                ImGui::SameLine();
                if (ImGui::Button("Frame >"))
                {
                    time = (std::min)(previewEnd, time + 1.0f / 30.0f);
                    syncPreviewTimes();
                    controlAnimation->socketPreviewPlaying = false;
                }
                ImGui::SameLine();
                if (ImGui::Button(">|"))
                {
                    time = previewEnd;
                    syncPreviewTimes();
                    controlAnimation->socketPreviewPlaying = false;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("%.3f / %.3f s", time, duration);
                if (ImGui::SliderFloat("##ControlTimelineSlider", &time,
                        0.0f, (std::max)(0.0001f, previewEnd), "%.3f s"))
                {
                    controlAnimation->socketPreviewPlaying = false;
                    syncPreviewTimes();
                }

                std::vector<std::string> controls;
                controls.reserve(controlClip->channels.size() + 7);
                std::string ikUpper;
                std::string ikForearm;
                std::string ikHand;
                if (controlModel->TryGetArmIKChain(false,
                        ikUpper, ikForearm, ikHand))
                {
                    controls.push_back(kAnimationLeftHandIKControl);
                    controls.push_back(kAnimationLeftElbowPoleControl);
                }
                if (controlModel->TryGetArmIKChain(true,
                        ikUpper, ikForearm, ikHand))
                {
                    controls.push_back(kAnimationRightHandIKControl);
                    controls.push_back(kAnimationRightElbowPoleControl);
                }
                std::string ikHead;
                if (controlModel->TryGetHeadIKBone(ikHead))
                    controls.push_back(kAnimationHeadLookIKControl);
                controls.push_back(kLeftFistControl);
                controls.push_back(kRightFistControl);
                for (const auto& channel : controlClip->channels)
                    if (std::find(controls.begin(), controls.end(),
                            channel.nodeName) == controls.end())
                        controls.push_back(channel.nodeName);
                // Some clips do not key every deform bone. Keep those bones in
                // the browser so a broken static pose can still be corrected.
                for (const auto& bone : controlModel->m_boneInfoMap)
                    if (std::find(controls.begin(), controls.end(), bone.first) ==
                        controls.end())
                        controls.push_back(bone.first);
                std::sort(controls.begin(), controls.end());
                const auto hasControl = [&](const std::string& name)
                {
                    return std::find(controls.begin(), controls.end(), name) != controls.end();
                };
                if (!hasControl(m_animationStudioBone))
                {
                    const char* preferred[] = {
                        kAnimationRightHandIKControl,
                        "mixamorig:RightHand", "mixamorig:Hips", "mixamorig:Spine" };
                    m_animationStudioBone.clear();
                    for (const char* name : preferred)
                        if (hasControl(name))
                        {
                            m_animationStudioBone = name;
                            break;
                        }
                    if (m_animationStudioBone.empty() && !controls.empty())
                        m_animationStudioBone = controls.front();
                }

                const float browserWidth = 235.0f;
                ImGui::BeginChild("##ControlBrowser", ImVec2(browserWidth, 330.0f), true);
                ImGui::TextColored(COL_ACCENT, "SKELETON CONTROLS");
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputTextWithHint("##ControlSearch", "Search controls...",
                    m_animationStudioControlSearch,
                    sizeof(m_animationStudioControlSearch));
                const char* groupNames[] = {
                    "Body", "Arms and Hands", "Legs and Feet", "Fingers" };
                for (int group = 0; group < 4; ++group)
                {
                    bool hasVisible = false;
                    for (const auto& name : controls)
                    {
                        const std::string label = AnimationControlLabel(name);
                        if (AnimationControlGroup(name) == group &&
                            (ContainsCaseInsensitive(name.c_str(),
                                m_animationStudioControlSearch) ||
                             ContainsCaseInsensitive(label.c_str(),
                                m_animationStudioControlSearch)))
                        {
                            hasVisible = true;
                            break;
                        }
                    }
                    if (!hasVisible) continue;
                    ImGui::SetNextItemOpen(group != 3, ImGuiCond_Once);
                    if (!ImGui::TreeNode(groupNames[group])) continue;
                    for (const auto& name : controls)
                    {
                        if (AnimationControlGroup(name) != group) continue;
                        const std::string label = AnimationControlLabel(name);
                        if (!ContainsCaseInsensitive(name.c_str(),
                                m_animationStudioControlSearch) &&
                            !ContainsCaseInsensitive(label.c_str(),
                                m_animationStudioControlSearch)) continue;
                        ImGui::PushID(name.c_str());
                        if (ImGui::Selectable(label.c_str(),
                                AnimationNodeNamesMatch(
                                    m_animationStudioBone, name)))
                            m_animationStudioBone = name;
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("%s", name.c_str());
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
                ImGui::EndChild();
                ImGui::SameLine();
                ImGui::BeginChild("##ControlDetails", ImVec2(0.0f, 330.0f), true);
                ImGui::TextColored(COL_ACCENT, "DETAILS");
                ImGui::Text("Control: %s",
                    AnimationControlLabel(m_animationStudioBone).c_str());
                ImGui::TextDisabled(IsAnimationIKControl(m_animationStudioBone)
                    ? "IK target | World-axis gizmo | Clip: %s"
                    : "Local additive transform | Clip: %s",
                    controlClip->name.c_str());
                const int variant = -1;
                if (IsFistControl(m_animationStudioBone))
                {
                    const bool rightHand = IsRightFistControl(m_animationStudioBone);
                    float amount = EvaluateFistControl(*controlAnimation,
                        controlClip->name, rightHand, variant, time);
                    const bool hasExactKey = FindFistKey(*controlAnimation,
                        controlClip->name, rightHand, variant, time) != nullptr;
                    ImGui::TextColored(hasExactKey ? COL_GREEN : COL_TEXT_DIM,
                        hasExactKey ? "KEY AT CURRENT TIME" :
                        "Interpolated fist (edit to create key)");
                    ImGui::TextWrapped("Captured from Blender %s. Positive values close the fist; negative values open it relative to the imported clip.",
                        rightHand ? "CTRL_Fist.R" : "CTRL_Fist.L");
                    if (ImGui::SliderFloat("Fist", &amount, -1.0f, 1.0f,
                            "%.2f"))
                    {
                        auto& key = EnsureFistKey(*controlAnimation,
                            controlClip->name, rightHand, variant, time, amount);
                        key.amount = amount;
                        controlAnimation->socketPreviewPlaying = false;
                        syncPreviewTimes();
                    }
                    const auto setFist = [&](float value)
                    {
                        auto& key = EnsureFistKey(*controlAnimation,
                            controlClip->name, rightHand, variant, time, value);
                        key.amount = value;
                        controlAnimation->socketPreviewPlaying = false;
                        syncPreviewTimes();
                    };
                    if (ImGui::Button("Open", ImVec2(90.0f, 0.0f))) setFist(-1.0f);
                    ImGui::SameLine();
                    if (ImGui::Button("Imported", ImVec2(90.0f, 0.0f))) setFist(0.0f);
                    ImGui::SameLine();
                    if (ImGui::Button("Close", ImVec2(90.0f, 0.0f))) setFist(1.0f);
                    if (ImGui::Button("Add Key")) setFist(amount);
                    ImGui::SameLine();
                    ImGui::BeginDisabled(!hasExactKey);
                    if (ImGui::Button("Delete Key"))
                    {
                        auto& keys = controlAnimation->fistKeys;
                        keys.erase(std::remove_if(keys.begin(), keys.end(),
                            [&](const AnimationFistKey& key)
                            {
                                return key.variant == variant &&
                                    key.clip == controlClip->name &&
                                    key.rightHand == rightHand &&
                                    std::abs(key.time - time) <= 0.0005f;
                            }), keys.end());
                    }
                    ImGui::EndDisabled();
                    if (ImGui::Button("Reset this fist for this clip",
                            ImVec2(-1.0f, 0.0f)))
                    {
                        auto& keys = controlAnimation->fistKeys;
                        keys.erase(std::remove_if(keys.begin(), keys.end(),
                            [&](const AnimationFistKey& key)
                            {
                                return key.variant == variant &&
                                    key.clip == controlClip->name &&
                                    key.rightHand == rightHand;
                            }), keys.end());
                    }
                    ImGui::TextDisabled("The virtual control drives all finger bones as one curve.");
                }
                else
                {
                DirectX::XMFLOAT3 position{};
                DirectX::XMFLOAT3 rotation{};
                const bool handIK = IsAnimationHandIKControl(
                    m_animationStudioBone);
                const bool elbowPole = IsAnimationElbowPoleControl(
                    m_animationStudioBone);
                const bool headIK = IsAnimationHeadIKControl(
                    m_animationStudioBone);
                const bool canTranslate = AnimationControlCanTranslate(
                    m_animationStudioBone);
                const bool canRotate = AnimationControlCanRotate(
                    m_animationStudioBone);
                const bool hasPoseTrack = EvaluatePoseCorrection(
                    *controlAnimation, controlClip->name,
                    m_animationStudioBone, variant, time, position, rotation);
                if (headIK && !hasPoseTrack)
                    position = { 0.0f, 0.0f, -1.0f };
                const bool hasExactKey = FindPoseKey(*controlAnimation,
                    controlClip->name, m_animationStudioBone,
                    variant, time) != nullptr;
                ImGui::TextColored(hasExactKey ? COL_GREEN : COL_TEXT_DIM,
                    hasExactKey ? "KEY AT CURRENT TIME" : "Interpolated pose (edit to create key)");
                if (handIK)
                    ImGui::TextWrapped("Move this target to pose the hand. The shoulder and elbow rotate through two-bone IK; limb lengths stay fixed.");
                if (elbowPole)
                    ImGui::TextWrapped("Move this pole to choose the direction of the elbow bend. It does not move or stretch the elbow bone directly.");
                if (headIK)
                    ImGui::TextWrapped("Move this target to aim the animated head. The runtime applies it after base/upper-body blending and limits the extra turn to 75 degrees.");
                ImGui::BeginDisabled(!canTranslate);
                bool positionChanged = ImGui::DragFloat3(
                    IsAnimationHeadIKControl(m_animationStudioBone)
                        ? "Look target offset (m)"
                        : IsAnimationArmIKControl(m_animationStudioBone)
                            ? "Target offset (m)" : "Location (m)",
                    &position.x, 0.002f, -5.0f, 5.0f, "%.3f");
                ImGui::EndDisabled();
                if (!canTranslate)
                    ImGui::TextDisabled("Joint translation is locked to prevent mesh stretching. Use rotation or a Hand IK Target.");
                ImGui::BeginDisabled(!canRotate);
                const bool rotationChanged = ImGui::DragFloat3(
                    handIK ? "Hand rotation (deg)" : "Rotation (deg)",
                    &rotation.x, 0.25f, -360.0f, 360.0f, "%.2f");
                ImGui::EndDisabled();
                const bool changed = positionChanged || rotationChanged;
                if (changed)
                {
                    if (m_animationStudioSnapEnabled)
                    {
                        position.x = SnapEditorValue(position.x,
                            m_animationStudioPositionSnap);
                        position.y = SnapEditorValue(position.y,
                            m_animationStudioPositionSnap);
                        position.z = SnapEditorValue(position.z,
                            m_animationStudioPositionSnap);
                        rotation.x = SnapEditorValue(rotation.x,
                            m_animationStudioRotationSnap);
                        rotation.y = SnapEditorValue(rotation.y,
                            m_animationStudioRotationSnap);
                        rotation.z = SnapEditorValue(rotation.z,
                            m_animationStudioRotationSnap);
                    }
                    AnimationPoseKey& key = EnsurePoseKey(*controlAnimation,
                        controlClip->name, m_animationStudioBone, variant,
                        time, position, rotation);
                    key.position = position;
                    key.rotation = rotation;
                    controlAnimation->socketPreviewPlaying = false;
                    syncPreviewTimes();
                }
                ImGui::Checkbox("Snap", &m_animationStudioSnapEnabled);
                if (m_animationStudioSnapEnabled)
                {
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(85.0f);
                    ImGui::DragFloat("Move", &m_animationStudioPositionSnap,
                        0.005f, 0.001f, 1.0f, "%.3f m");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(85.0f);
                    ImGui::DragFloat("Rotate", &m_animationStudioRotationSnap,
                        1.0f, 1.0f, 90.0f, "%.0f deg");
                }
                if (!canTranslate)
                    m_socketStudioGizmoMode = 1;
                if (!canRotate)
                    m_socketStudioGizmoMode = 0;
                ImGui::BeginDisabled(!canTranslate);
                ImGui::RadioButton("Move gizmo", &m_socketStudioGizmoMode, 0);
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::BeginDisabled(!canRotate);
                ImGui::RadioButton("Rotate gizmo", &m_socketStudioGizmoMode, 1);
                ImGui::EndDisabled();
                if (!ImGui::GetIO().WantTextInput)
                {
                    if (canTranslate && ImGui::IsKeyPressed(ImGuiKey_W, false))
                        m_socketStudioGizmoMode = 0;
                    if (canRotate && ImGui::IsKeyPressed(ImGuiKey_E, false))
                        m_socketStudioGizmoMode = 1;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("W / E");
                if (ImGui::Button("Add Key"))
                    EnsurePoseKey(*controlAnimation, controlClip->name,
                        m_animationStudioBone, variant, time, position, rotation);
                ImGui::SameLine();
                ImGui::BeginDisabled(!hasExactKey);
                if (ImGui::Button("Delete Key"))
                {
                    auto& keys = controlAnimation->poseKeys;
                    keys.erase(std::remove_if(keys.begin(), keys.end(),
                        [&](const AnimationPoseKey& key)
                        {
                            return key.variant == variant &&
                                key.clip == controlClip->name &&
                                AnimationNodeNamesMatch(
                                    key.bone, m_animationStudioBone) &&
                                std::abs(key.time - time) <= 0.0005f;
                        }), keys.end());
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Copy"))
                {
                    m_animationStudioCopiedPosition = position;
                    m_animationStudioCopiedRotation = rotation;
                    m_animationStudioCopiedTransform = true;
                }
                ImGui::SameLine();
                ImGui::BeginDisabled(!m_animationStudioCopiedTransform);
                if (ImGui::Button("Paste"))
                {
                    AnimationPoseKey& key = EnsurePoseKey(*controlAnimation,
                        controlClip->name, m_animationStudioBone, variant, time,
                        m_animationStudioCopiedPosition,
                        m_animationStudioCopiedRotation);
                    key.position = m_animationStudioCopiedPosition;
                    key.rotation = m_animationStudioCopiedRotation;
                }
                ImGui::EndDisabled();
                if (ImGui::Button("Reset this control for this clip", ImVec2(-1.0f, 0.0f)))
                {
                    auto& keys = controlAnimation->poseKeys;
                    keys.erase(std::remove_if(keys.begin(), keys.end(),
                        [&](const AnimationPoseKey& key)
                        {
                            return key.clip == controlClip->name &&
                                AnimationNodeNamesMatch(
                                    key.bone, m_animationStudioBone) &&
                                key.variant == variant;
                        }), keys.end());
                }
                ImGui::TextDisabled("Viewport gizmo writes a key automatically at the playhead.");
                }
                ImGui::EndChild();

                const ImVec2 timelineStart = ImGui::GetCursorScreenPos();
                const float timelineWidth = ImGui::GetContentRegionAvail().x;
                const float timelineHeight = 28.0f;
                ImGui::InvisibleButton("##ControlTimeline",
                    ImVec2(timelineWidth, timelineHeight));
                ImDrawList* timelineDraw = ImGui::GetWindowDrawList();
                const ImVec2 timelineEnd(timelineStart.x + timelineWidth,
                    timelineStart.y + timelineHeight);
                timelineDraw->AddRectFilled(timelineStart, timelineEnd,
                    IM_COL32(38, 38, 42, 255), 3.0f);
                timelineDraw->AddRect(timelineStart, timelineEnd,
                    IM_COL32(85, 85, 92, 255), 3.0f);
                if (IsFistControl(m_animationStudioBone))
                {
                    const bool rightHand = IsRightFistControl(m_animationStudioBone);
                    for (const auto& key : controlAnimation->fistKeys)
                        if (key.variant == variant && key.clip == controlClip->name &&
                            key.rightHand == rightHand)
                        {
                            const float x = timelineStart.x + timelineWidth *
                                std::clamp(key.time / duration, 0.0f, 1.0f);
                            timelineDraw->AddCircleFilled(
                                ImVec2(x, timelineStart.y + timelineHeight * 0.5f),
                                4.0f, IM_COL32(190, 105, 255, 255));
                        }
                }
                else
                    for (const auto& key : controlAnimation->poseKeys)
                        if (key.variant == variant && key.clip == controlClip->name &&
                            AnimationNodeNamesMatch(key.bone, m_animationStudioBone))
                        {
                            const float x = timelineStart.x + timelineWidth *
                                std::clamp(key.time / duration, 0.0f, 1.0f);
                            timelineDraw->AddCircleFilled(
                                ImVec2(x, timelineStart.y + timelineHeight * 0.5f),
                                4.0f, IM_COL32(245, 190, 45, 255));
                        }
                const float playheadX = timelineStart.x + timelineWidth *
                    std::clamp(time / duration, 0.0f, 1.0f);
                timelineDraw->AddLine(ImVec2(playheadX, timelineStart.y),
                    ImVec2(playheadX, timelineEnd.y),
                    IM_COL32(35, 155, 255, 255), 2.0f);
                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
                {
                    time = (std::min)(previewEnd,
                        std::clamp((ImGui::GetIO().MousePos.x - timelineStart.x) /
                            (std::max)(1.0f, timelineWidth), 0.0f, 1.0f) * duration);
                    controlAnimation->socketPreviewPlaying = false;
                    syncPreviewTimes();
                }
                ImGui::TextDisabled(IsFistControl(m_animationStudioBone)
                    ? "Purple: fist keys | Blue: playhead"
                    : "Yellow: keys for selected control | Blue: playhead");
            }
            else
                ImGui::TextColored(COL_RED, "This model has no animation clips.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("State Machine"))
        {
            ImGui::TextWrapped("Assign imported clips to gameplay states.");
            ImGui::TextWrapped("Search 1 / 2 / 3: optional full-body clips played in order while an unarmed NPC waits in Searching. Empty slots are skipped.");
            ImGui::Checkbox("Gameplay crouch pose", &animation->crouchPose);
            ImGui::TextDisabled("Animation stance only; the physics capsule keeps its standing size.");
            InputStringValue("Upper-body layer root##state_machine", animation->upperBodyRoot);
            if (ImGui::Button("Return to gameplay##state_machine"))
                animation->socketPreviewEnabled = false;
            ImGui::Text("Locomotion: %s", AnimationStateName(animation->currentState));
            ImGui::Text("Base clip: %d | Upper clip: %d",
                animation->animationIndex, animation->upperBodyAnimationIndex);
            for (auto& binding : animation->animations)
            {
                ImGui::PushID(static_cast<int>(binding.state));
                const char* label = AnimationStateName(binding.state);
                ImGui::TextUnformatted(label);
                ImGui::SameLine(155.0f);
                const char* current = binding.name.empty() ? "(weapon default / empty)"
                    : binding.name.c_str();
                if (ImGui::BeginCombo("##clip", current))
                {
                    if (ImGui::Selectable("(empty)", binding.name.empty()))
                    {
                        binding.name.clear();
                        binding.index = -1;
                    }
                    for (size_t i = 0; i < model->GetAnimationCount(); ++i)
                    {
                        const auto* candidate = model->GetAnimation(static_cast<int>(i));
                        if (candidate && ImGui::Selectable(candidate->name.c_str(),
                            candidate->name == binding.name))
                        {
                            binding.name = candidate->name;
                            binding.index = static_cast<int>(i);
                        }
                    }
                    ImGui::EndCombo();
                }
                if (!binding.name.empty())
                {
                    DrawClipSpeed(*animation, binding.name, "Playback speed");
                    if (ImGui::Button("Preview state"))
                    {
                        const int clip = model->FindAnimationIndexBySemanticName(binding.name);
                        if (model->GetAnimation(clip))
                        {
                            animation->socketPreviewEnabled = true;
                            animation->socketPreviewPunch = false;
                            animation->socketPreviewBlendEnabled = false;
                            animation->socketPreviewClip = clip;
                            animation->socketPreviewTime = 0.0f;
                            animation->socketPreviewPlaying = true;
                        }
                    }
                }
                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Keyframes"))
        {
            const bool firstPersonKeys = !npcTarget && m_cameraController &&
                m_cameraController->IsFirstPerson() && armsAnimation && armsModel;
            auto* keyAnimation = firstPersonKeys ? armsAnimation : animation;
            const auto* keyModel = firstPersonKeys ? armsModel : model;
            const bool punchKeys = animation->socketPreviewPunch;
            const int keyVariant = punchKeys
                ? animation->socketPreviewPunchVariant : -1;
            ImGui::TextDisabled(firstPersonKeys ? "Editing FIRST PERSON arms"
                : "Editing THIRD PERSON character");
            if (punchKeys)
                ImGui::Text("Gameplay attack %d", keyVariant + 1);
            const bool upperLayerKeys = !punchKeys &&
                keyAnimation->socketPreviewBlendEnabled &&
                m_animationStudioKeyLayer == 1 &&
                keyModel->GetAnimation(keyAnimation->socketPreviewUpperClip);
            if (!punchKeys && keyAnimation->socketPreviewBlendEnabled)
                ImGui::TextColored(upperLayerKeys ? COL_ACCENT : COL_GREEN,
                    "Mixed preview | Writing %s layer keys",
                    upperLayerKeys ? "UPPER BODY" : "BASE");
            const auto* clip = keyModel->GetAnimation(punchKeys
                ? keyAnimation->upperBodyAnimationIndex
                : upperLayerKeys ? keyAnimation->socketPreviewUpperClip
                    : keyAnimation->socketPreviewClip);
            if (!clip)
                ImGui::TextDisabled("Select a clip in Controls or Preview first.");
            else
            {
                const auto clipHasNode = [&](const std::string& name)
                {
                    return std::any_of(clip->channels.begin(), clip->channels.end(),
                        [&](const auto& channel)
                        {
                            return AnimationNodeNamesMatch(channel.nodeName, name);
                        });
                };
                if (!clipHasNode(m_animationStudioBone) && !clip->channels.empty())
                    m_animationStudioBone = clip->channels.front().nodeName;
                if (ImGui::BeginCombo("Bone", m_animationStudioBone.c_str()))
                {
                    std::vector<std::string> bones;
                    for (const auto& channel : clip->channels)
                        if (std::find(bones.begin(), bones.end(), channel.nodeName) == bones.end())
                            bones.push_back(channel.nodeName);
                    std::sort(bones.begin(), bones.end());
                    for (const auto& name : bones)
                        if (ImGui::Selectable(name.c_str(), name == m_animationStudioBone))
                            m_animationStudioBone = name;
                    ImGui::EndCombo();
                }
                keyAnimation->socketPreviewPlaying = false;
                if (punchKeys) animation->socketPreviewPlaying = false;
                const float rate = clip->ticksPerSecond > 0.0f
                    ? clip->ticksPerSecond : 24.0f;
                const float duration = clip->duration / rate;
                const float previewEnd = AnimationClipPreviewEnd(duration);
                float& keyTime = upperLayerKeys
                    ? keyAnimation->socketPreviewUpperTime
                    : keyAnimation->socketPreviewTime;
                keyTime = std::clamp(keyTime, 0.0f, previewEnd);
                const auto syncEditedTime = [&](float time)
                {
                    if (punchKeys)
                    {
                        if (firstPersonKeys)
                        {
                            const auto* worldClip = model->GetAnimation(
                                animation->upperBodyAnimationIndex);
                            if (worldClip && duration > 0.0f)
                            {
                                const float worldRate = worldClip->ticksPerSecond > 0.0f
                                    ? worldClip->ticksPerSecond : 24.0f;
                                animation->socketPreviewTime = time / duration *
                                    (worldClip->duration / worldRate);
                            }
                        }
                        else animation->socketPreviewTime = time;
                        return;
                    }
                    if (keyAnimation->socketPreviewBlendEnabled &&
                        keyAnimation->socketPreviewSyncLayerPhase)
                    {
                        const auto* base = keyModel->GetAnimation(
                            keyAnimation->socketPreviewClip);
                        const auto* upper = keyModel->GetAnimation(
                            keyAnimation->socketPreviewUpperClip);
                        if (base && upper)
                        {
                            const float baseRate = base->ticksPerSecond > 0.0f
                                ? base->ticksPerSecond : 24.0f;
                            const float upperRate = upper->ticksPerSecond > 0.0f
                                ? upper->ticksPerSecond : 24.0f;
                            const float baseDuration = base->duration / baseRate;
                            const float upperDuration = upper->duration / upperRate;
                            if (baseDuration > 0.00001f && upperDuration > 0.00001f)
                            {
                                if (upperLayerKeys)
                                    keyAnimation->socketPreviewTime =
                                        time / upperDuration * baseDuration;
                                else
                                    keyAnimation->socketPreviewUpperTime =
                                        time / baseDuration * upperDuration;
                            }
                        }
                    }
                    keyAnimation->currentTime = keyAnimation->socketPreviewTime;
                    keyAnimation->upperBodyTime =
                        keyAnimation->socketPreviewUpperTime;
                };
                if (ImGui::SliderFloat("Time (s)##keys", &keyTime,
                    0.0f, (std::max)(0.0001f, previewEnd), "%.3f"))
                    syncEditedTime(keyTime);
                AnimationPoseKey* active = FindPoseKey(*keyAnimation,
                    clip->name, m_animationStudioBone, keyVariant,
                    keyTime, 0.005f);
                if (!active && ImGui::Button("Add key at current time"))
                {
                    DirectX::XMFLOAT3 position{};
                    DirectX::XMFLOAT3 rotation{};
                    EvaluatePoseCorrection(*keyAnimation, clip->name,
                        m_animationStudioBone, keyVariant,
                        keyTime, position, rotation);
                    active = &EnsurePoseKey(*keyAnimation, clip->name,
                        m_animationStudioBone, keyVariant,
                        keyTime, position, rotation);
                }
                if (active)
                {
                    ImGui::DragFloat3("Position offset (m)", &active->position.x,
                        0.002f, -2.0f, 2.0f);
                    ImGui::DragFloat3("Rotation offset (deg)", &active->rotation.x,
                        0.25f, -180.0f, 180.0f);
                    if (ImGui::Button("Delete current key"))
                    {
                        const auto index = static_cast<size_t>(
                            active - keyAnimation->poseKeys.data());
                        keyAnimation->poseKeys.erase(keyAnimation->poseKeys.begin() + index);
                    }
                }
                ImGui::SeparatorText("Keys for selected bone");
                for (const auto& key : keyAnimation->poseKeys)
                    if (key.variant == keyVariant && key.clip == clip->name &&
                        AnimationNodeNamesMatch(key.bone, m_animationStudioBone))
                    {
                        ImGui::PushID(&key);
                        const std::string label = std::to_string(key.time) + " s";
                        if (ImGui::Selectable(label.c_str(),
                            std::abs(key.time - keyTime) < 0.005f))
                        {
                            keyTime = (std::min)(key.time, previewEnd);
                            syncEditedTime(keyTime);
                        }
                        ImGui::PopID();
                    }
                ImGui::TextDisabled("Keys add offsets to the imported GLB animation.");
            }
            ImGui::EndTabItem();
        }
        if (weapon && ImGui::BeginTabItem("Weapon & IK"))
        {
            const bool firstPerson = !npcTarget && m_cameraController &&
                m_cameraController->IsFirstPerson();
            ImGui::Checkbox("Show world weapon", &weapon->showWorldVisual);
            InputStringValue("Weapon model path", weapon->visualModelPath);
            InputStringValue("Weapon hand bone", weapon->rightHandBone);
            DrawWeaponBallisticsEditor(*weapon);
            Entity poseEntity = studioTarget;
            if (firstPerson)
            {
                poseEntity = INVALID_ENTITY;
                registry.ForEach<FirstPersonViewComponent>(
                    [&](Entity entity, FirstPersonViewComponent& view)
                    {
                        if (view.owner == studioTarget && view.followCamera)
                            poseEntity = entity;
                    });
            }
            auto* poseAnimation = registry.GetComponent<AnimationComponent>(poseEntity);
            const auto* poseMesh = registry.GetComponent<MeshRendererComponent>(poseEntity);
            const AssimpModelClass* poseModel = poseMesh ? poseMesh->model : nullptr;
            WeaponGripTransform& activeGrip = firstPerson
                ? weapon->firstPersonGrip : weapon->worldGrip;
            ImGui::TextColored(COL_TEXT_DIM, "Editing %s grip",
                firstPerson ? "FIRST PERSON" : "THIRD PERSON");
            ImGui::BeginDisabled(firstPerson);
            if (ImGui::Button("Import sockets from Blender export",
                    ImVec2(-1.0f, 0.0f)))
                sceneManager.ImportBlenderWeaponSockets(
                    *weapon, m_weaponProfileStatus);
            ImGui::EndDisabled();
            ImGui::TextDisabled(firstPerson
                ? "Blender socket import updates the third-person grip only."
                : "Reads Kevin_Combat_Game.sockets.json; profile stays unchanged until Save.");
            ImGui::Separator();
            const char* sockets[] = { "Right hand / weapon", "Left hand target", "Muzzle" };
            if (ImGui::BeginCombo("Selected socket", sockets[m_socketStudioSocket]))
            {
                for (int socket = 0; socket < 3; ++socket)
                {
                    if (ImGui::Selectable(sockets[socket], m_socketStudioSocket == socket))
                        m_socketStudioSocket = socket;
                }
                ImGui::EndCombo();
            }
            ImGui::BeginDisabled(firstPerson || m_socketStudioPreviewGameCamera);
            if (ImGui::Button("Focus selected socket", ImVec2(-1.0f, 0.0f)) &&
                m_cameraController)
            {
                const auto* playerTransform =
                    registry.GetComponent<TransformComponent>(studioTarget);
                DirectX::XMMATRIX hand;
                if (playerTransform && model->TryGetBoneModelTransform(
                        *animation, weapon->rightHandBone, hand))
                {
                    using namespace DirectX;
                    const XMMATRIX gun = WeaponIKSystem::GripMatrix(activeGrip) *
                        hand * playerTransform->GetWorldMatrix();
                    XMMATRIX selected = gun;
                    if (m_socketStudioSocket == 1)
                    {
                        const auto& p = weapon->leftHandGrip;
                        selected = XMMatrixTranslation(p.x, p.y, p.z) * gun;
                    }
                    if (m_socketStudioSocket == 2)
                    {
                        const auto& p = weapon->muzzlePosition;
                        selected = XMMatrixTranslation(p.x, p.y, p.z) * gun;
                    }
                    XMFLOAT3 world{};
                    XMStoreFloat3(&world, selected.r[3]);
                    const auto& origin = playerTransform->position;
                    m_cameraController->SetSocketStudioTargetOffset(
                        { world.x - origin.x, world.y - origin.y, world.z - origin.z });
                    m_cameraController->SetSocketStudioDistance(1.2f);
                }
            }
            ImGui::EndDisabled();
            if (m_socketStudioSocket != 2)
            {
                ImGui::RadioButton("Move", &m_socketStudioGizmoMode, 0);
                ImGui::SameLine();
                ImGui::RadioButton("Rotate", &m_socketStudioGizmoMode, 1);
            }
            else
                m_socketStudioGizmoMode = 0;
            ImGui::TextDisabled("Drag the colored axes in the 3D view or edit numbers below.");
            ImGui::Separator();

            if (m_socketStudioSocket == 0)
            {
                if (!firstPerson && weapon->enableLeftHandIK)
                {
                    ImGui::Checkbox(
                        "Keep left hand in place while moving weapon",
                        &m_socketStudioKeepLeftHandInPlace);
                    ImGui::TextDisabled(
                        "When enabled, weapon controls do not drag the IK hand target.");
                }

                // До изменения оружия запоминаем цель кисти в пространстве модели.
                // После изменения пересчитаем её обратно относительно винтовки.
                DirectX::XMMATRIX fixedLeftTargetModel = DirectX::XMMatrixIdentity();
                bool canKeepLeftTarget = false;
                if (!firstPerson && m_socketStudioKeepLeftHandInPlace &&
                    weapon->enableLeftHandIK && poseModel && poseAnimation)
                {
                    DirectX::XMMATRIX oldRightHand;
                    if (poseModel->TryGetBoneModelTransform(
                            *poseAnimation, weapon->rightHandBone, oldRightHand))
                    {
                        using namespace DirectX;
                        const XMMATRIX oldWeaponModel =
                            WeaponIKSystem::GripMatrix(activeGrip) * oldRightHand;
                        const auto& p = weapon->leftHandGrip;
                        const auto& r = weapon->leftHandRotation;
                        fixedLeftTargetModel = XMMatrixRotationRollPitchYaw(
                            r.x, r.y, r.z) *
                            XMMatrixTranslation(p.x, p.y, p.z) *
                            oldWeaponModel;
                        canKeepLeftTarget = true;
                    }
                }

                bool gripChanged = false;
                if (poseModel && ImGui::BeginCombo("Parent bone", weapon->rightHandBone.c_str()))
                {
                    std::vector<std::string> bones;
                    for (const auto& [name, info] : poseModel->m_boneInfoMap)
                        bones.push_back(name);
                    std::sort(bones.begin(), bones.end());
                    for (const std::string& name : bones)
                        if (ImGui::Selectable(name.c_str(), name == weapon->rightHandBone))
                        {
                            weapon->rightHandBone = name;
                            gripChanged = true;
                        }
                    ImGui::EndCombo();
                }
                DirectX::XMMATRIX bone;
                if (!poseModel || !poseAnimation ||
                    !poseModel->TryGetBoneModelTransform(
                        *poseAnimation, weapon->rightHandBone, bone))
                    ImGui::TextColored(COL_RED, "Parent bone is missing from the current pose.");
                gripChanged |= ImGui::DragFloat3(
                    "Position (m)", &activeGrip.position.x, 0.002f);
                DirectX::XMFLOAT3 degrees = {
                    DirectX::XMConvertToDegrees(activeGrip.rotation.x),
                    DirectX::XMConvertToDegrees(activeGrip.rotation.y),
                    DirectX::XMConvertToDegrees(activeGrip.rotation.z) };
                if (ImGui::DragFloat3("Rotation (deg)", &degrees.x, 0.25f))
                {
                    activeGrip.rotation = {
                        DirectX::XMConvertToRadians(degrees.x),
                        DirectX::XMConvertToRadians(degrees.y),
                        DirectX::XMConvertToRadians(degrees.z) };
                    gripChanged = true;
                }
                auto pivot = activeGrip.pivot;
                if (ImGui::DragFloat3("Rotation center in weapon (m)",
                        &pivot.x, 0.002f))
                {
                    SetGripPivotPreservingPose(activeGrip, pivot);
                    gripChanged = true;
                }
                if (ImGui::Button("Center on left wrist target"))
                {
                    SetGripPivotPreservingPose(activeGrip, firstPerson
                        ? weapon->firstPersonLeftHandGrip : weapon->leftHandGrip);
                    gripChanged = true;
                }
                ImGui::TextDisabled("Rotate gizmo uses the rifle's local axes and this center.");
                ImGui::TextDisabled("Rotation (deg) is Euler relative to the hand bone.");
                static float localStep = 2.0f;
                ImGui::DragFloat("Local turn step (deg)", &localStep,
                    0.1f, 0.1f, 45.0f, "%.1f");
                for (int axis = 0; axis < 3; ++axis)
                {
                    ImGui::PushID(axis);
                    const char* axisName = axis == 0 ? "X" : axis == 1 ? "Y" : "Z";
                    ImGui::Text("Weapon %s", axisName);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("-"))
                    {
                        RotateGripInWeaponSpace(activeGrip, axis, -localStep);
                        gripChanged = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("+"))
                    {
                        RotateGripInWeaponSpace(activeGrip, axis, localStep);
                        gripChanged = true;
                    }
                    ImGui::PopID();
                }
                float scale = activeGrip.scale.x;
                if (ImGui::DragFloat("Uniform scale", &scale, 0.005f, 0.01f, 10.0f))
                {
                    activeGrip.scale = { scale, scale, scale };
                    gripChanged = true;
                }

                if (gripChanged && canKeepLeftTarget)
                {
                    DirectX::XMMATRIX newRightHand;
                    if (poseModel->TryGetBoneModelTransform(
                            *poseAnimation, weapon->rightHandBone, newRightHand))
                    {
                        const DirectX::XMMATRIX newWeaponModel =
                            WeaponIKSystem::GripMatrix(activeGrip) * newRightHand;
                        SetLeftWristTargetFromMatrix(
                            *weapon,
                            fixedLeftTargetModel * DirectX::XMMatrixInverse(
                                nullptr, newWeaponModel),
                            false);
                    }
                }
            }
            else if (m_socketStudioSocket == 1)
            {
                bool& enableIK = firstPerson
                    ? weapon->enableFirstPersonLeftHandIK : weapon->enableLeftHandIK;
                auto& wristPosition = firstPerson
                    ? weapon->firstPersonLeftHandGrip : weapon->leftHandGrip;
                auto& wristRotation = firstPerson
                    ? weapon->firstPersonLeftHandRotation : weapon->leftHandRotation;
                if (ImGui::Checkbox("Enable left hand IK", &enableIK) && enableIK &&
                    poseModel && poseAnimation)
                    CaptureLeftWristTarget(*weapon, poseModel, poseAnimation, firstPerson);
                if (ImGui::Button("Capture from current wrist pose", ImVec2(-1.0f, 0.0f)))
                    m_weaponProfileStatus = CaptureLeftWristTarget(
                        *weapon, poseModel, poseAnimation, firstPerson)
                        ? "Left wrist target captured." : "Could not read the wrist bones.";
                ImGui::DragFloat3("Position in weapon (m)", &wristPosition.x, 0.002f);
                DirectX::XMFLOAT3 degrees = {
                    DirectX::XMConvertToDegrees(wristRotation.x),
                    DirectX::XMConvertToDegrees(wristRotation.y),
                    DirectX::XMConvertToDegrees(wristRotation.z) };
                if (ImGui::DragFloat3("Rotation (deg)", &degrees.x, 0.25f))
                    wristRotation = {
                        DirectX::XMConvertToRadians(degrees.x),
                        DirectX::XMConvertToRadians(degrees.y),
                        DirectX::XMConvertToRadians(degrees.z) };
                if (poseAnimation)
                    ImGui::Text("IK: %s | wrist error: %.3f m",
                        poseAnimation->leftHandIKApplied ? "active" : "inactive",
                        poseAnimation->leftHandIKError);
                else
                    ImGui::TextColored(COL_RED, "No animated arms for this view.");
            }
            else
            {
                ImGui::TextDisabled("Muzzle position is shared by both views of this weapon model.");
                ImGui::DragFloat3("Position in weapon (m)", &weapon->muzzlePosition.x, 0.002f);
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Preview"))
        {
            if (m_cameraController)
            {
                int view = m_cameraController->IsFirstPerson() ? 1 : 0;
                if (ImGui::RadioButton("Third person", view == 0))
                    m_cameraController->SetPlayViewMode(CameraController::PlayViewMode::ThirdPerson);
                if (!npcTarget)
                {
                    ImGui::SameLine();
                    if (ImGui::RadioButton("First person", view == 1))
                        m_cameraController->SetPlayViewMode(
                            CameraController::PlayViewMode::FirstPerson);
                    if (ImGui::Checkbox("Preview game camera",
                            &m_socketStudioPreviewGameCamera) &&
                        m_socketStudioPreviewGameCamera)
                        m_cameraController->SetSocketStudioFreeFly(false);
                }
                else
                    ImGui::TextDisabled(
                        "NPC preview is always third-person and has no FP camera.");
                bool freeFly = m_cameraController->IsSocketStudioFreeFly();
                if (ImGui::Checkbox("Free fly camera", &freeFly))
                {
                    m_cameraController->SetSocketStudioFreeFly(freeFly);
                    if (freeFly) m_socketStudioPreviewGameCamera = false;
                }
                if (freeFly)
                    ImGui::TextDisabled("RMB: look. WASD: move. R/F: up/down. Shift: faster.");
                if (m_cameraController->IsFirstPerson() && armsView && freeFly)
                {
                    ImGui::Checkbox("Keep FP arms in place while camera moves",
                        &m_socketStudioFreezeFirstPersonArms);
                    armsView->studioFreezeCameraAnchor =
                        m_socketStudioFreezeFirstPersonArms;
                    ImGui::TextDisabled(m_socketStudioFreezeFirstPersonArms
                        ? "FP arms are fixed in the scene; fly around them to inspect the pose."
                        : "FP arms follow the free camera like they do during gameplay.");
                }
                else if (armsView)
                    armsView->studioFreezeCameraAnchor = false;
                m_cameraController->SetSocketStudioEnabled(
                    m_socketStudioOpen && !m_socketStudioPreviewGameCamera && !freeFly);
                ImGui::TextDisabled("Save weapon profile to keep both camera views and arm distance.");
                if (!npcTarget && playCamera)
                {
                    ImGui::SeparatorText("Rendering");
                    ImGui::DragFloat("Render distance (m)",
                        &playCamera->renderDistance, 1.0f, 1.0f, 10000.0f, "%.0f");
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip(
                            "Far clipping distance of the gameplay camera.\n"
                            "A larger value shows more of the world but can cost performance.");
                    if (m_cameraController->IsFirstPerson())
                    {
                        ImGui::SeparatorText("First-person camera position");
                        ImGui::DragFloat("Eye right (m)",
                            &playCamera->firstPersonEyeOffset.x, 0.01f, -1.0f, 1.0f);
                        ImGui::DragFloat("Eye height (m)",
                            &playCamera->firstPersonEyeOffset.y, 0.01f, 0.1f, 3.0f);
                        ImGui::DragFloat("Eye forward (m)",
                            &playCamera->firstPersonEyeOffset.z, 0.01f, -1.0f, 1.0f);
                        if (armsView)
                        {
                            if (ImGui::TreeNode("Shared base arms position (all clips)"))
                            {
                                ImGui::DragFloat3("Base arms relative to eye (m)",
                                    &armsView->cameraOffset.x, 0.01f);
                                ImGui::TextDisabled("This also affects Idle and weapon animations.");
                                ImGui::TreePop();
                            }
                        }
                        if (ImGui::Button("Reset FP camera"))
                            playCamera->firstPersonEyeOffset = { 0.0f, 1.6f, 0.15f };
                    }
                    else
                    {
                        ImGui::SeparatorText("Third-person game camera");
                        ImGui::DragFloat("Follow distance (m)",
                            &playCamera->thirdPersonDistance, 0.05f, 1.0f, 30.0f);
                        ImGui::DragFloat("Target right (m)",
                            &playCamera->thirdPersonTargetOffset.x, 0.01f, -2.0f, 2.0f);
                        ImGui::DragFloat("Target height (m)",
                            &playCamera->thirdPersonTargetOffset.y, 0.01f, 0.1f, 3.0f);
                        ImGui::DragFloat("Target forward (m)",
                            &playCamera->thirdPersonTargetOffset.z, 0.01f, -2.0f, 2.0f);
                        if (ImGui::Button("Reset third-person camera"))
                        {
                            playCamera->thirdPersonDistance = 4.0f;
                            playCamera->thirdPersonTargetOffset = { 0.0f, 1.5f, 0.0f };
                        }
                    }
                }
                if (m_cameraController->IsFirstPerson() && armsView)
                {
                    ImGui::SeparatorText("Vertical look limits");
                    if (ImGui::SliderFloat("Look up", &armsView->lookUpLimit,
                            0.0f, 89.0f, "%.1f deg"))
                        m_cameraController->SetFirstPersonLookUpLimit(
                            armsView->lookUpLimit);
                    if (ImGui::SliderFloat("Look down", &armsView->lookDownLimit,
                            0.0f, 89.0f, "%.1f deg"))
                        m_cameraController->SetFirstPersonLookDownLimit(
                            armsView->lookDownLimit);
                }
                if (freeFly)
                {
                    ImGui::TextDisabled("Camera moves freely when the cursor is captured.");
                }
                else if (m_socketStudioPreviewGameCamera)
                {
                    ImGui::TextDisabled("Right-drag in view: turn game camera.");
                    float yaw = m_cameraController->GetFollowYaw();
                    if (ImGui::DragFloat("Game yaw", &yaw, 0.5f, -360.0f, 360.0f, "%.1f deg"))
                        m_cameraController->SetFollowYaw(yaw);
                    float pitch = m_cameraController->GetFollowPitch();
                    if (ImGui::DragFloat("Game pitch", &pitch, 0.5f, -89.0f, 89.0f, "%.1f deg"))
                        m_cameraController->SetFollowPitch(pitch);
                }
                else
                {
                    ImGui::TextDisabled("Right-drag: orbit. Wheel: zoom. Middle-drag: pan.");
                    float yaw = m_cameraController->GetSocketStudioYaw();
                    if (ImGui::DragFloat("Orbit yaw", &yaw, 0.5f, -360.0f, 360.0f, "%.1f deg"))
                        m_cameraController->SetSocketStudioYaw(yaw);
                    float pitch = m_cameraController->GetSocketStudioPitch();
                    const float minPitch = m_cameraController->IsFirstPerson()
                        ? -m_cameraController->GetFirstPersonLookUpLimit() : -85.0f;
                    const float maxPitch = m_cameraController->IsFirstPerson()
                        ? m_cameraController->GetFirstPersonLookDownLimit() : 85.0f;
                    if (ImGui::DragFloat("Orbit pitch", &pitch, 0.5f,
                            minPitch, maxPitch, "%.1f deg"))
                        m_cameraController->SetSocketStudioPitch(pitch);
                    float distance = m_cameraController->GetSocketStudioDistance();
                    if (ImGui::DragFloat("Orbit distance", &distance, 0.02f, 0.35f, 15.0f, "%.2f m"))
                        m_cameraController->SetSocketStudioDistance(distance);
                    auto target = m_cameraController->GetSocketStudioTargetOffset();
                    if (ImGui::DragFloat3("Orbit target offset", &target.x, 0.01f))
                        m_cameraController->SetSocketStudioTargetOffset(target);
                    if (ImGui::Button("Yaw 0")) m_cameraController->SetSocketStudioYaw(0.0f);
                    ImGui::SameLine();
                    if (ImGui::Button("Yaw 90")) m_cameraController->SetSocketStudioYaw(90.0f);
                    ImGui::SameLine();
                    if (ImGui::Button("Yaw 180")) m_cameraController->SetSocketStudioYaw(180.0f);
                }
            }
            ImGui::SeparatorText("Animation pose");
            ImGui::Checkbox("Preview selected clip", &m_socketStudioClipPreview);
            const bool firstPersonPreview = !npcTarget && m_cameraController &&
                m_cameraController->IsFirstPerson() && armsAnimation && armsModel;
            if (firstPersonPreview)
            {
                const bool sourceModeChanged = ImGui::Checkbox(
                    "Use 3P clip with FP camera and arm settings",
                    &m_socketStudioPreviewWorldClipInFirstPerson);
                if (sourceModeChanged)
                {
                    if (m_socketStudioPreviewWorldClipInFirstPerson)
                    {
                        const auto* fpClip = armsModel->GetAnimation(
                            armsAnimation->socketPreviewClip);
                        const int worldIndex = fpClip
                            ? model->FindAnimationIndexByName(fpClip->name) : -1;
                        if (worldIndex >= 0)
                            animation->socketPreviewClip = worldIndex;
                        animation->socketPreviewTime = armsAnimation->socketPreviewTime;
                        animation->socketPreviewPlaying = armsAnimation->socketPreviewPlaying;
                    }
                    else
                    {
                        const auto* worldClip = model->GetAnimation(
                            animation->socketPreviewClip);
                        const int fpIndex = worldClip
                            ? armsModel->FindAnimationIndexByName(worldClip->name) : -1;
                        if (fpIndex >= 0)
                            armsAnimation->socketPreviewClip = fpIndex;
                        armsAnimation->socketPreviewTime = animation->socketPreviewTime;
                        armsAnimation->socketPreviewPlaying = animation->socketPreviewPlaying;
                    }
                }
                ImGui::TextDisabled(m_socketStudioPreviewWorldClipInFirstPerson
                    ? "One 3P animation is sampled on the FP arms; FP framing and pose keys are applied on top."
                    : "Select and preview a clip directly from the FP arms model.");
            }
            const bool worldClipOnFirstPerson = firstPersonPreview &&
                m_socketStudioPreviewWorldClipInFirstPerson;
            animation->socketPreviewFirstPersonFromWorld = worldClipOnFirstPerson;
            auto* previewAnimation = worldClipOnFirstPerson
                ? animation : (firstPersonPreview ? armsAnimation : animation);
            const auto* previewModel = worldClipOnFirstPerson
                ? model : (firstPersonPreview ? armsModel : model);
            ImGui::TextDisabled(worldClipOnFirstPerson
                ? "THIRD PERSON clip -> FIRST PERSON arms"
                : firstPersonPreview ? "FIRST PERSON arms"
                : "THIRD PERSON character");
            if (weapon)
                ImGui::Checkbox("Apply weapon hand IK to preview",
                    &animation->socketPreviewApplyIK);
            const auto* selectedClip = previewModel->GetAnimation(previewAnimation->socketPreviewClip);
            if (ImGui::BeginCombo("Clip", selectedClip
                ? selectedClip->name.c_str() : "Select clip"))
            {
                for (size_t i = 0; i < previewModel->GetAnimationCount(); ++i)
                {
                    const auto* candidate = previewModel->GetAnimation(static_cast<int>(i));
                    if (candidate && ImGui::Selectable(candidate->name.c_str(),
                        previewAnimation->socketPreviewClip == static_cast<int>(i)))
                    {
                        animation->socketPreviewPunch = false;
                        m_socketStudioClipPreview = true;
                        previewAnimation->socketPreviewClip = static_cast<int>(i);
                        previewAnimation->socketPreviewTime = 0.0f;
                        previewAnimation->currentTime = 0.0f;
                    }
                }
                ImGui::EndCombo();
            }
            const auto* clip = previewModel->GetAnimation(previewAnimation->socketPreviewClip);
            if (clip)
            {
                const float rate = clip->ticksPerSecond > 0.0f ? clip->ticksPerSecond : 24.0f;
                const float duration = clip->duration / rate;
                ImGui::Checkbox("Play clip", &previewAnimation->socketPreviewPlaying);
                if (ImGui::SliderFloat("Time (s)", &previewAnimation->socketPreviewTime,
                    0.0f, (std::max)(0.001f, duration), "%.3f"))
                {
                    previewAnimation->socketPreviewPlaying = false;
                    previewAnimation->currentTime = previewAnimation->socketPreviewTime;
                }
                ImGui::Text("Clip: %s", clip->name.c_str());
                DrawClipSpeed(*animation, clip->name, "Selected clip speed");
                if (firstPersonPreview)
                {
                    const int matchingFP = armsModel->FindAnimationIndexByName(
                        clip->name);
                    if (worldClipOnFirstPerson && matchingFP < 0)
                        ImGui::TextColored(COL_RED,
                            "The FP arms model has no matching clip named '%s'.",
                            clip->name.c_str());
                    else if (worldClipOnFirstPerson)
                        ImGui::TextColored(COL_GREEN,
                            "Matched FP arms clip; playhead follows the 3P action.");
                    else
                        ImGui::TextDisabled("Gameplay 1P follows the 3P action clock.");
                }
                if (firstPersonPreview)
                    DrawFirstPersonClipFraming(*animation, clip->name, -1,
                        "Position for selected 1P clip");
            }
            else
                ImGui::TextColored(COL_RED, "No animation clip selected.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Combat"))
        {
            ImGui::TextWrapped(npcTarget
                ? "NPC attacks use these weighted world clips when its combat state requests Punch."
                : "Unarmed: hold RMB for guard, click LMB to attack. With a rifle, LMB fires.");
            auto& variants = animation->punchVariants;
            if (variants.empty()) variants.push_back({ "Left jab", "Kevin_Melee_Jab_L",
                "Kevin_Melee_Jab_L", false, -1.0f, 1.0f });
            m_punchStudioVariant = std::clamp(m_punchStudioVariant, 0,
                static_cast<int>(variants.size()) - 1);
            const char* variantLabel = variants[m_punchStudioVariant].name.c_str();
            if (ImGui::BeginCombo("Attack variant", variantLabel))
            {
                for (size_t i = 0; i < variants.size(); ++i)
                    if (ImGui::Selectable(variants[i].name.c_str(),
                        m_punchStudioVariant == static_cast<int>(i)))
                    {
                        m_punchStudioVariant = static_cast<int>(i);
                        animation->socketPreviewPunchVariant = m_punchStudioVariant;
                        animation->socketPreviewTime = 0.0f;
                        animation->socketPreviewPlaying = false;
                    }
                ImGui::EndCombo();
            }
            if (ImGui::Button("Add attack") && variants.size() < 8)
            {
                variants.push_back({ "Punch " + std::to_string(variants.size() + 1), "", "" });
                m_punchStudioVariant = static_cast<int>(variants.size()) - 1;
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(variants.size() <= 1);
            if (ImGui::Button("Remove attack"))
            {
                const int removed = m_punchStudioVariant;
                variants.erase(variants.begin() + m_punchStudioVariant);
                const auto shiftKeys = [removed](AnimationComponent* target)
                {
                    if (!target) return;
                    auto& keys = target->poseKeys;
                    keys.erase(std::remove_if(keys.begin(), keys.end(),
                        [removed](const AnimationPoseKey& key)
                        { return key.variant == removed; }), keys.end());
                    for (auto& key : keys)
                        if (key.variant > removed) --key.variant;
                };
                shiftKeys(animation);
                shiftKeys(armsAnimation);
                m_punchStudioVariant = std::clamp(m_punchStudioVariant, 0,
                    static_cast<int>(variants.size()) - 1);
                animation->socketPreviewPunch = false;
            }
            ImGui::EndDisabled();
            auto& variant = variants[m_punchStudioVariant];
            InputStringValue("Attack name", variant.name);
            auto clipCombo = [](const char* label, std::string& name,
                const AssimpModelClass* source,
                const char* emptyLabel = "Use Punch binding")
            {
                if (!source) return;
                const char* current = name.empty() ? emptyLabel : name.c_str();
                if (ImGui::BeginCombo(label, current))
                {
                    if (ImGui::Selectable(emptyLabel, name.empty()))
                        name.clear();
                    for (size_t i = 0; i < source->GetAnimationCount(); ++i)
                    {
                        const auto* candidate = source->GetAnimation(static_cast<int>(i));
                        if (candidate && ImGui::Selectable(candidate->name.c_str(),
                            name == candidate->name))
                            name = candidate->name;
                    }
                    ImGui::EndCombo();
                }
            };
            ImGui::SeparatorText("Guard action (RMB held, no weapon)");
            ImGui::Checkbox("Enable guard", &animation->meleeActions.guardEnabled);
            clipCombo(npcTarget ? "Guard clip" : "3P guard clip",
                animation->meleeActions.guardWorldClip, model,
                "(none)");
            if (!npcTarget)
                clipCombo("1P guard clip",
                    animation->meleeActions.guardFirstPersonClip,
                    armsModel, "(none)");
            DrawClipSpeed(*animation, animation->meleeActions.guardWorldClip,
                "Guard speed");
            clipCombo("3P moving guard (optional)",
                animation->meleeActions.guardMoveWorldClip, model,
                "(locomotion + guard)");
            if (!npcTarget)
                clipCombo("1P moving guard (optional)",
                    animation->meleeActions.guardMoveFirstPersonClip, armsModel,
                    "(locomotion + guard)");
            DrawClipSpeed(*animation, animation->meleeActions.guardMoveWorldClip,
                "Moving guard speed");
            ImGui::TextDisabled("Empty moving clip keeps locomotion with guard arms.");
            if (m_cameraController && m_cameraController->IsFirstPerson())
                DrawFirstPersonClipFraming(*animation,
                    animation->meleeActions.guardFirstPersonClip, -1,
                    "1P guard position");
            if (!npcTarget)
                ImGui::Checkbox("LMB attack requires guard",
                    &animation->meleeActions.attackRequiresGuard);
            ImGui::SeparatorText("Weighted attack action (LMB pressed)");
            ImGui::SeparatorText("Animation sources");
            ImGui::TextDisabled(npcTarget
                ? "NPC actions use the world model only."
                : "3P is visible to other players. 1P is local camera arms only.");
            clipCombo(npcTarget ? "Attack clip" : "3P punch clip (world)",
                variant.worldClip, model);
            clipCombo(npcTarget ? "NPC base under attack" : "Base under attack (3P)",
                animation->meleeActions.attackBaseWorldClip, model,
                "(combat guard)");
            ImGui::TextDisabled(
                "This full-body clip supplies the lower layer while the attack is blended from the upper-body root.");
            ImGui::TextDisabled("Empty attack base uses the combat guard for the legs and hips.");
            if (!npcTarget)
                clipCombo("1P punch clip (local only)",
                    variant.firstPersonClip, armsModel);
            std::string attackSpeedClip = variant.worldClip;
            if (attackSpeedClip.empty())
                for (const auto& binding : animation->animations)
                    if (binding.state == AnimationState::Punch)
                    { attackSpeedClip = binding.name; break; }
            DrawClipSpeed(*animation, attackSpeedClip, "Selected attack speed");
            if (!npcTarget)
                ImGui::TextDisabled(
                    "1P punch follows this attack speed and hit timing.");
            if (m_cameraController && m_cameraController->IsFirstPerson())
                DrawFirstPersonClipFraming(*animation, variant.firstPersonClip,
                    m_punchStudioVariant, "1P position for this attack");
            if (!npcTarget && !armsModel)
                ImGui::TextColored(COL_RED,
                    "No First Person View model is linked to this player.");
            ImGui::Text("Attack hand:");
            ImGui::SameLine();
            int attackHand = variant.rightHand ? 1 : 0;
            if (ImGui::RadioButton("Left##AttackHand", &attackHand, 0))
                variant.rightHand = false;
            ImGui::SameLine();
            if (ImGui::RadioButton("Right##AttackHand", &attackHand, 1))
                variant.rightHand = true;
            ImGui::TextColored(COL_ACCENT, "Active IK chain: %sArm -> %sForeArm -> %sHand",
                variant.rightHand ? "Right" : "Left",
                variant.rightHand ? "Right" : "Left",
                variant.rightHand ? "Right" : "Left");
            ImGui::DragFloat("Selection weight", &variant.weight, 0.05f, 0.0f, 100.0f,
                "%.2f");
            float totalWeight = 0.0f;
            for (const auto& option : variants)
                totalWeight += (std::max)(0.0f, option.weight);
            ImGui::Text("Chance on LMB: %.1f%%", totalWeight > 0.0f
                ? 100.0f * (std::max)(0.0f, variant.weight) / totalWeight : 0.0f);
            bool customContact = variant.contactPhase >= 0.0f;
            if (ImGui::Checkbox("Custom hit moment", &customContact))
                variant.contactPhase = customContact
                    ? (playerData ? playerData->punchContactPhase : 0.5f) : -1.0f;
            if (customContact)
                ImGui::SliderFloat("Hit moment", &variant.contactPhase, 0.05f, 0.95f);
            ImGui::SeparatorText("Punch IK");
            bool customIKStrength = variant.ikStrength >= 0.0f;
            if (ImGui::Checkbox("Custom IK strength", &customIKStrength))
                variant.ikStrength = customIKStrength
                    ? (playerData ? playerData->punchIKStrength : 1.0f) : -1.0f;
            if (customIKStrength)
                ImGui::SliderFloat("IK strength", &variant.ikStrength,
                    0.0f, 1.0f);
            const float inheritedContact = variant.contactPhase >= 0.0f
                ? variant.contactPhase
                : (playerData ? playerData->punchContactPhase : 0.55f);
            const float envelopeContact = std::clamp(inheritedContact,
                0.05f, 0.95f);
            variant.ikStartPhase = std::clamp(variant.ikStartPhase,
                0.0f, envelopeContact - 0.01f);
            variant.ikEndPhase = std::clamp(variant.ikEndPhase,
                envelopeContact + 0.01f, 1.0f);
            ImGui::SliderFloat("IK starts (phase)", &variant.ikStartPhase,
                0.0f, envelopeContact - 0.01f, "%.2f");
            ImGui::SliderFloat("IK ends (phase)", &variant.ikEndPhase,
                envelopeContact + 0.01f, 1.0f, "%.2f");
            ImGui::TextDisabled("IK fades in to the hit moment %.2f and fades out afterwards.",
                envelopeContact);
            ImGui::DragFloat3("Fist contact offset (m)",
                &variant.fistOffset.x, 0.002f, -0.5f, 0.5f, "%.3f");
            ImGui::DragFloat3("Fist rotation offset (deg)",
                &variant.handRotation.x, 0.25f, -180.0f, 180.0f, "%.2f");
            ImGui::DragFloat3("Aim offset (m)", &variant.aimOffset.x,
                0.002f, -1.0f, 1.0f, "%.3f");
            ImGui::SliderFloat("Arm extension", &variant.armExtension,
                0.5f, 0.99f, "%.2f");
            const float poleLengthSq =
                variant.elbowPoleOffset.x * variant.elbowPoleOffset.x +
                variant.elbowPoleOffset.y * variant.elbowPoleOffset.y +
                variant.elbowPoleOffset.z * variant.elbowPoleOffset.z;
            bool customElbowDirection = poleLengthSq > 0.000001f;
            if (ImGui::Checkbox("Override elbow bend direction",
                    &customElbowDirection))
            {
                // Down is a useful neutral starting point for a punch and, more
                // importantly, is never confused with the old zero/inherit value.
                variant.elbowPoleOffset = customElbowDirection
                    ? XMFLOAT3{ 0.0f, -1.0f, 0.0f } : XMFLOAT3{};
            }
            if (customElbowDirection)
            {
                ImGui::DragFloat3("Elbow bend direction (model)",
                    &variant.elbowPoleOffset.x, 0.01f, -1.0f, 1.0f, "%.3f");
                const float outward = variant.rightHand ? -1.0f : 1.0f;
                if (ImGui::Button("Bend down"))
                    variant.elbowPoleOffset = { 0.0f, -1.0f, 0.0f };
                ImGui::SameLine();
                if (ImGui::Button("Down + outward"))
                    variant.elbowPoleOffset = { outward, -1.0f, 0.0f };
                ImGui::SameLine();
                if (ImGui::Button("Outward"))
                    variant.elbowPoleOffset = { outward, 0.0f, 0.0f };
            }
            ImGui::TextDisabled(
                "Rotation changes only the wrist/fist after arm IK; the source clip stays intact.");
            ImGui::TextDisabled(
                "Aim: X sideways, Y vertical, Z depth. Lower arm extension prevents a locked elbow.");
            ImGui::TextDisabled(
                "Elbow direction is measured from the shoulder in model space; its length is ignored.");
            ImGui::TextDisabled(
                "Zero/inherit preserves the source animation. The override rotates the whole arm bend plane.");
            int mirrorSource = -1;
            for (int index = 0; index < static_cast<int>(variants.size()); ++index)
                if (index != m_punchStudioVariant &&
                    variants[index].rightHand != variant.rightHand)
                {
                    mirrorSource = index;
                    break;
                }
            if (mirrorSource >= 0)
            {
                const std::string mirrorLabel = std::string("Mirror IK from ") +
                    variants[mirrorSource].name;
                if (ImGui::Button(mirrorLabel.c_str(), ImVec2(-1.0f, 0.0f)))
                {
                    const auto& source = variants[mirrorSource];
                    variant.ikStrength = source.ikStrength;
                    variant.fistOffset = { -source.fistOffset.x,
                        source.fistOffset.y, source.fistOffset.z };
                    variant.handRotation = { source.handRotation.x,
                        -source.handRotation.y, -source.handRotation.z };
                    variant.aimOffset = { -source.aimOffset.x,
                        source.aimOffset.y, source.aimOffset.z };
                    variant.elbowPoleOffset = { -source.elbowPoleOffset.x,
                        source.elbowPoleOffset.y, source.elbowPoleOffset.z };
                    variant.armExtension = source.armExtension;
                    variant.ikStartPhase = source.ikStartPhase;
                    variant.ikEndPhase = source.ikEndPhase;
                }
            }
            if (ImGui::Button("Reset IK for this attack", ImVec2(-1.0f, 0.0f)))
            {
                variant.ikStrength = -1.0f;
                variant.fistOffset = {};
                variant.handRotation = {};
                variant.aimOffset = {};
                variant.elbowPoleOffset = {};
                variant.armExtension = 0.92f;
                variant.ikStartPhase = 0.0f;
                variant.ikEndPhase = 1.0f;
            }
            ImGui::SeparatorText("Preview aim");
            ImGui::Checkbox("Preview aim follows camera",
                &animation->socketPreviewPunchUseCameraAim);
            if (!animation->socketPreviewPunchUseCameraAim)
                ImGui::SliderFloat("Fixed aim distance (m)",
                    &animation->socketPreviewPunchAimDistance,
                    0.1f, 5.0f, "%.2f");
            const float previewAimError = animation->socketPreviewPunchAimError;
            const ImVec4 aimErrorColor = previewAimError <= 0.05f
                ? COL_GREEN : previewAimError <= 0.15f
                    ? COL_ORANGE : COL_RED;
            ImGui::TextColored(aimErrorColor, "Fist aim error: %.3f m (%s hand)",
                previewAimError, variant.rightHand ? "right" : "left");
            if (customIKStrength && variant.ikStrength <= 0.001f)
                ImGui::TextColored(COL_ORANGE,
                    "IK strength is zero: aim, elbow, and extension controls are inactive.");
            if (ImGui::Button("Preview gameplay punch", ImVec2(-1.0f, 0.0f)))
            {
                m_socketStudioClipPreview = false;
                animation->socketPreviewPunch = true;
                animation->socketPreviewPunchVariant = m_punchStudioVariant;
                animation->socketPreviewPlaying = false;
                animation->socketPreviewTime = 0.0f;
                animation->socketPreviewPunchAimError = 0.0f;
                animation->upperBodyTime = 0.0f;
                animation->socketPreviewApplyIK = false;
                m_socketStudioPreviewGameCamera = !npcTarget;
                if (m_cameraController)
                {
                    m_cameraController->SetSocketStudioFreeFly(false);
                    m_cameraController->SetSocketStudioEnabled(false);
                }
                if (armsAnimation) armsAnimation->socketPreviewEnabled = true;
            }
            if (animation->socketPreviewPunch)
            {
                animation->socketPreviewPunchVariant = m_punchStudioVariant;
                ImGui::BeginDisabled(
                    !animation->meleeActions.attackBaseWorldClip.empty() ||
                    !animation->meleeActions.guardWorldClip.empty());
                ImGui::Combo("Base movement", &animation->socketPreviewPunchMovement,
                    "Idle\0Walk\0Run\0");
                ImGui::EndDisabled();
                if (!animation->meleeActions.attackBaseWorldClip.empty())
                    ImGui::TextColored(COL_GREEN,
                        "Gameplay base: %s",
                        animation->meleeActions.attackBaseWorldClip.c_str());
                else
                    ImGui::TextColored(COL_GREEN, "Gameplay base (guard): %s",
                        animation->meleeActions.guardWorldClip.c_str());
                const auto* activeClip = model->GetAnimation(
                    animation->upperBodyAnimationIndex);
                if (activeClip)
                {
                    const float rate = activeClip->ticksPerSecond > 0.0f
                        ? activeClip->ticksPerSecond : 24.0f;
                    const float duration = activeClip->duration / rate;
                    ImGui::Checkbox("Play attack", &animation->socketPreviewPlaying);
                    if (ImGui::SliderFloat("Attack time (s)",
                        &animation->socketPreviewTime, 0.0f,
                        (std::max)(0.001f, duration), "%.3f"))
                        animation->socketPreviewPlaying = false;
                    ImGui::Text("Runtime clip: %s", activeClip->name.c_str());
                }
                else
                    ImGui::TextColored(COL_RED, "World punch clip is missing.");
            }
            ImGui::TextDisabled("Keyframes edit this variant in the current camera view.");
            if (playerData)
            {
                DrawPunchSettings(*playerData);
                ImGui::SeparatorText("Live punch");
                ImGui::Text("Active: %s", playerData->punchActive ? "yes" : "no");
                ImGui::Text("Attack variant: %d", playerData->punchVariant + 1);
                ImGui::Text("Time: %.2f / %.2f", playerData->punchTime,
                    playerData->punchDuration);
                ImGui::Text("Fist to crosshair ray: %.3f m", playerData->punchAimError);
                ImGui::Text("Last hit entity: %s",
                    EditorEntityDebugLabel(registry, playerData->punchLastHit).c_str());
            }
            ImGui::EndTabItem();
        }
        if (!npcTarget && weapon && ImGui::BeginTabItem("FP Weapon"))
        {
            ImGui::TextDisabled("Use Camera / Pose > First person for live preview.");
            if (armsView)
            {
                ImGui::DragFloat3("Shared arms camera offset (all clips)",
                    &armsView->cameraOffset.x, 0.01f);
                ImGui::TextDisabled("Affects Idle too. Use clip positions in Punch / Camera tabs.");
            }
            else
                ImGui::TextColored(COL_RED, "No first-person arms linked to the player.");
            ImGui::Separator();
            ImGui::DragFloat3("FP position", &weapon->firstPersonGrip.position.x, 0.002f);
            DirectX::XMFLOAT3 degrees = {
                DirectX::XMConvertToDegrees(weapon->firstPersonGrip.rotation.x),
                DirectX::XMConvertToDegrees(weapon->firstPersonGrip.rotation.y),
                DirectX::XMConvertToDegrees(weapon->firstPersonGrip.rotation.z) };
            if (ImGui::DragFloat3("FP rotation (deg)", &degrees.x, 0.25f))
                weapon->firstPersonGrip.rotation = {
                    DirectX::XMConvertToRadians(degrees.x),
                    DirectX::XMConvertToRadians(degrees.y),
                    DirectX::XMConvertToRadians(degrees.z) };
            auto fpPivot = weapon->firstPersonGrip.pivot;
            if (ImGui::DragFloat3("FP rotation center (m)", &fpPivot.x, 0.002f))
                SetGripPivotPreservingPose(weapon->firstPersonGrip, fpPivot);
            ImGui::DragFloat3("FP scale", &weapon->firstPersonGrip.scale.x, 0.005f, 0.01f, 10.0f);
            ImGui::SeparatorText("Left hand IK for first person");
            auto* fpAnimation = registry.GetComponent<AnimationComponent>(armsEntity);
            const auto* fpMesh = registry.GetComponent<MeshRendererComponent>(armsEntity);
            const AssimpModelClass* fpModel = fpMesh ? fpMesh->model : nullptr;
            if (ImGui::Checkbox("Enable FP left hand IK", &weapon->enableFirstPersonLeftHandIK) &&
                weapon->enableFirstPersonLeftHandIK)
                CaptureLeftWristTarget(*weapon, fpModel, fpAnimation, true);
            if (ImGui::Button("Capture FP wrist from current pose"))
                m_weaponProfileStatus = CaptureLeftWristTarget(*weapon, fpModel, fpAnimation, true)
                    ? "FP wrist target captured." : "Could not read FP wrist bones.";
            ImGui::DragFloat3("FP left wrist target (m)",
                &weapon->firstPersonLeftHandGrip.x, 0.002f);
            DirectX::XMFLOAT3 wristDegrees = {
                DirectX::XMConvertToDegrees(weapon->firstPersonLeftHandRotation.x),
                DirectX::XMConvertToDegrees(weapon->firstPersonLeftHandRotation.y),
                DirectX::XMConvertToDegrees(weapon->firstPersonLeftHandRotation.z) };
            if (ImGui::DragFloat3("FP left wrist rotation (deg)",
                    &wristDegrees.x, 0.25f))
                weapon->firstPersonLeftHandRotation = {
                    DirectX::XMConvertToRadians(wristDegrees.x),
                    DirectX::XMConvertToRadians(wristDegrees.y),
                    DirectX::XMConvertToRadians(wristDegrees.z) };
            if (fpAnimation)
                ImGui::Text("FP IK: %s | wrist error: %.3f m",
                    fpAnimation->leftHandIKApplied ? "active" : "inactive",
                    fpAnimation->leftHandIKError);
            ImGui::DragFloat("Arms pitch scale", &weapon->firstPersonPitchScale, 0.01f, -2.0f, 2.0f);
            float offset = DirectX::XMConvertToDegrees(weapon->firstPersonPitchOffset);
            if (ImGui::DragFloat("Arms pitch offset", &offset, 0.25f, -90.0f, 90.0f, "%.1f deg"))
                weapon->firstPersonPitchOffset = DirectX::XMConvertToRadians(offset);
            if (ImGui::Button("Copy third-person grip to FP", ImVec2(-1.0f, 0.0f)))
                weapon->firstPersonGrip = weapon->worldGrip;
            ImGui::TextDisabled("Save weapon profile to keep arms offset, FP grip and IK.");
            ImGui::EndTabItem();
        }
        if (weapon && ImGui::BeginTabItem("Weapon Profile"))
        {
            DrawWeaponProfileControls(sceneManager, *weapon, animation,
                m_weaponProfileStatus, m_weaponVisualReloadRequested);
            if (ImGui::Button("Reload visual model", ImVec2(-1.0f, 0.0f)))
                m_weaponVisualReloadRequested = true;
            DrawWeaponGripEditor(*weapon, animation, model, &m_weaponVisualReloadRequested);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Save / Export"))
    {
        ImGui::TextDisabled("Animation preset: %s", quickAnimationPath.c_str());
        if (weapon)
        {
            const std::string weaponPath =
                SceneManager::WeaponProfilePath(weapon->gripProfileName);
            ImGui::BeginDisabled(weaponPath.empty());
            if (ImGui::Button("Save Weapon Profile", ImVec2(-1.0f, 0.0f)))
                m_weaponProfileStatus = sceneManager.SaveWeaponProfile(*weapon)
                    ? "Saved weapon profile: " + weaponPath
                    : "Could not save weapon profile.";
            ImGui::EndDisabled();
            ImGui::BeginDisabled(quickAnimationPath.empty() || weaponPath.empty());
            if (ImGui::Button("Save Animation + Weapon", ImVec2(-1.0f, 0.0f)))
            {
                const bool weaponSaved = sceneManager.SaveWeaponProfile(*weapon);
                const bool animationSaved = SaveAnimationStudio(quickAnimationPath,
                    *animation, armsAnimation, playerData, npcTarget);
                if (animationSaved)
                {
                    animation->profileName = m_animationStudioProfileName;
                    if (armsAnimation)
                        armsAnimation->profileName = m_animationStudioProfileName;
                }
                m_weaponProfileStatus = weaponSaved && animationSaved
                    ? "Saved animation and weapon profiles."
                    : "Could not save every studio profile.";
            }
            ImGui::EndDisabled();
        }
    }
    ImGui::End();
}

void EditorUI::RenderSocketStudioGizmo(SceneManager& sceneManager,
    const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection)
{
    if (!m_socketStudioOpen || !m_cameraController)
        return;

    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 display = io.DisplaySize;
    const float panelWidth = std::clamp(display.x * 0.46f, 620.0f, 820.0f);
    if (m_socketStudioUiCursorActive && !io.WantCaptureMouse &&
        io.MousePos.x >= 0.0f && io.MousePos.x < display.x - panelWidth)
    {
        if (!m_cameraController->IsSocketStudioFreeFly() &&
            ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f))
        {
            if (m_socketStudioPreviewGameCamera)
            {
                m_cameraController->SetFollowYaw(
                    m_cameraController->GetFollowYaw() + io.MouseDelta.x * 0.4f);
                m_cameraController->SetFollowPitch(
                    m_cameraController->GetFollowPitch() + io.MouseDelta.y * 0.4f);
            }
            else
            {
                m_cameraController->SetSocketStudioYaw(
                    m_cameraController->GetSocketStudioYaw() + io.MouseDelta.x * 0.4f);
                m_cameraController->SetSocketStudioPitch(
                    m_cameraController->GetSocketStudioPitch() + io.MouseDelta.y * 0.4f);
            }
        }
        if (!m_cameraController->IsSocketStudioFreeFly() &&
            !m_socketStudioPreviewGameCamera && io.MouseWheel != 0.0f)
            m_cameraController->SetSocketStudioDistance(
                m_cameraController->GetSocketStudioDistance() *
                std::pow(0.85f, io.MouseWheel));
        if (!m_cameraController->IsSocketStudioFreeFly() &&
            !m_socketStudioPreviewGameCamera &&
            ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))
        {
            auto target = m_cameraController->GetSocketStudioTargetOffset();
            const float yaw = DirectX::XMConvertToRadians(
                m_cameraController->GetSocketStudioYaw());
            const float metersPerPixel =
                m_cameraController->GetSocketStudioDistance() * 0.0015f;
            target.x -= std::cos(yaw) * io.MouseDelta.x * metersPerPixel;
            target.z += std::sin(yaw) * io.MouseDelta.x * metersPerPixel;
            target.y += io.MouseDelta.y * metersPerPixel;
            m_cameraController->SetSocketStudioTargetOffset(target);
        }
    }
    auto& registry = sceneManager.GetECSRegistry();
    Entity player = INVALID_ENTITY;
    registry.ForEach<PlayerGameTag>([&](Entity candidate, PlayerGameTag&)
    {
        if (player == INVALID_ENTITY) player = candidate;
    });
    Entity studioTarget = m_animationStudioTarget;
    if (studioTarget == INVALID_ENTITY || !registry.IsAlive(studioTarget))
        studioTarget = player;
    const bool npcTarget = registry.HasComponent<NPCComponent>(studioTarget) &&
        !registry.HasComponent<PlayerGameTag>(studioTarget);
    using namespace DirectX;

    if (m_animationStudioControllersActive)
    {
        if (!m_socketStudioClipPreview || m_animationStudioBone.empty())
            return;
        const bool firstPersonControl = !npcTarget &&
            m_animationStudioControllerView == 1;
        Entity poseEntity = studioTarget;
        if (firstPersonControl)
        {
            poseEntity = INVALID_ENTITY;
            registry.ForEach<FirstPersonViewComponent>(
                [&](Entity entity, FirstPersonViewComponent& viewModel)
                {
                    if (viewModel.owner == studioTarget && viewModel.followCamera)
                        poseEntity = entity;
                });
        }
        auto* poseAnimation = registry.GetComponent<AnimationComponent>(poseEntity);
        const auto* poseMesh = registry.GetComponent<MeshRendererComponent>(poseEntity);
        const auto* poseTransform = registry.GetComponent<TransformComponent>(poseEntity);
        if (!poseAnimation || !poseMesh || !poseMesh->model || !poseTransform)
            return;
        const bool editUpperLayer = poseAnimation->socketPreviewBlendEnabled &&
            m_animationStudioKeyLayer == 1 &&
            poseMesh->model->GetAnimation(poseAnimation->socketPreviewUpperClip);
        const int editClipIndex = editUpperLayer
            ? poseAnimation->socketPreviewUpperClip
            : poseAnimation->socketPreviewClip;
        const auto* clip = poseMesh->model->GetAnimation(editClipIndex);
        if (!clip) return;

        // Keep the gizmo sampler aligned with the same composed pose shown in
        // the viewport, while writing the correction to the selected source layer.
        poseAnimation->animationIndex = poseAnimation->socketPreviewClip;
        poseAnimation->upperBodyAnimationIndex =
            poseAnimation->socketPreviewBlendEnabled &&
            poseMesh->model->GetAnimation(poseAnimation->socketPreviewUpperClip)
            ? poseAnimation->socketPreviewUpperClip : -1;
        poseAnimation->poseVariant = -1;
        // CTRL_Fist is a virtual multi-bone curve edited in the Details panel;
        // it has no single transform for a viewport gizmo.
        if (IsFistControl(m_animationStudioBone))
            return;
        XMMATRIX currentModel;
        XMMATRIX uncorrectedModel;
        if (IsAnimationHeadIKControl(m_animationStudioBone))
        {
            std::string head;
            if (!poseMesh->model->TryGetHeadIKBone(head) ||
                !poseMesh->model->TryGetAnimationNodeEditTransforms(
                    *poseAnimation, head, uncorrectedModel, currentModel))
                return;
            XMFLOAT3 position{};
            XMFLOAT3 rotation{};
            const bool hasTrack = EvaluatePoseCorrection(*poseAnimation,
                clip->name, m_animationStudioBone, -1,
                editUpperLayer ? poseAnimation->socketPreviewUpperTime
                    : poseAnimation->socketPreviewTime,
                position, rotation);
            if (!hasTrack)
                position = { 0.0f, 0.0f, -1.0f };
            currentModel = XMMatrixTranslation(position.x, position.y,
                position.z) * uncorrectedModel;
        }
        else if (IsAnimationArmIKControl(m_animationStudioBone))
        {
            std::string upperArm;
            std::string forearm;
            std::string hand;
            const bool elbowPole = IsAnimationElbowPoleControl(
                m_animationStudioBone);
            const bool rightSide = elbowPole
                ? IsRightAnimationElbowPoleControl(m_animationStudioBone)
                : IsRightAnimationHandIKControl(m_animationStudioBone);
            if (!poseMesh->model->TryGetArmIKChain(
                    rightSide,
                    upperArm, forearm, hand) ||
                !poseMesh->model->TryGetAnimationNodeEditTransforms(
                    *poseAnimation, elbowPole ? forearm : hand,
                    uncorrectedModel, currentModel))
                return;

            // The virtual track is an additive transform relative to the
            // imported hand pose at this playhead position.
            XMFLOAT3 position{};
            XMFLOAT3 rotation{};
            EvaluatePoseCorrection(*poseAnimation, clip->name,
                m_animationStudioBone, -1,
                editUpperLayer ? poseAnimation->socketPreviewUpperTime
                    : poseAnimation->socketPreviewTime,
                position, rotation);
            const XMMATRIX correction = (elbowPole
                ? XMMatrixIdentity()
                : XMMatrixRotationRollPitchYaw(
                    XMConvertToRadians(rotation.x), XMConvertToRadians(rotation.y),
                    XMConvertToRadians(rotation.z))) *
                XMMatrixTranslation(position.x, position.y, position.z);
            currentModel = correction * uncorrectedModel;
        }
        else if (!poseMesh->model->TryGetAnimationNodeEditTransforms(
                     *poseAnimation, m_animationStudioBone,
                     currentModel, uncorrectedModel))
            return;
        const XMMATRIX entityWorld = poseTransform->GetWorldMatrix();
        const XMMATRIX controllerWorld = currentModel * entityWorld;
        const XMMATRIX viewProjection = view * projection;
        const XMVECTOR projected = XMVector3Transform(
            controllerWorld.r[3], viewProjection);
        const float projectedW = XMVectorGetW(projected);
        if (projectedW > 0.0001f)
        {
            const float x = XMVectorGetX(projected) / projectedW;
            const float y = XMVectorGetY(projected) / projectedW;
            const float z = XMVectorGetZ(projected) / projectedW;
            if (z >= 0.0f && z <= 1.0f &&
                std::abs(x) <= 1.0f && std::abs(y) <= 1.0f)
            {
                const ImVec2 screen{ (x * 0.5f + 0.5f) * display.x,
                    (1.0f - (y * 0.5f + 0.5f)) * display.y };
                ImDrawList* draw = ImGui::GetForegroundDrawList();
                draw->AddCircleFilled(screen, 6.0f,
                    IM_COL32(35, 155, 255, 255));
                const std::string label =
                    AnimationControlLabel(m_animationStudioBone);
                draw->AddText(ImVec2(screen.x + 9.0f, screen.y - 8.0f),
                    IM_COL32(120, 205, 255, 255), label.c_str());
            }
        }
        if (!m_socketStudioUiCursorActive)
            return;

        XMFLOAT4X4 edited{};
        XMStoreFloat4x4(&edited, controllerWorld);
        ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
        ImGuizmo::SetRect(0.0f, 0.0f, display.x, display.y);
        const bool canTranslate = AnimationControlCanTranslate(
            m_animationStudioBone);
        const bool canRotate = AnimationControlCanRotate(
            m_animationStudioBone);
        if (!canTranslate)
            m_socketStudioGizmoMode = 1;
        if (!canRotate)
            m_socketStudioGizmoMode = 0;
        const ImGuizmo::OPERATION operation = m_socketStudioGizmoMode == 0
            ? ImGuizmo::TRANSLATE : ImGuizmo::ROTATE;
        float snap[3] = {
            operation == ImGuizmo::TRANSLATE ? m_animationStudioPositionSnap
                : m_animationStudioRotationSnap,
            operation == ImGuizmo::TRANSLATE ? m_animationStudioPositionSnap
                : m_animationStudioRotationSnap,
            operation == ImGuizmo::TRANSLATE ? m_animationStudioPositionSnap
                : m_animationStudioRotationSnap };
        const ImGuizmo::MODE gizmoSpace =
            operation == ImGuizmo::TRANSLATE &&
            IsAnimationIKControl(m_animationStudioBone)
            ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
        if (!ImGuizmo::Manipulate(reinterpret_cast<const float*>(&view),
                reinterpret_cast<const float*>(&projection), operation,
                gizmoSpace, &edited._11, nullptr,
                m_animationStudioSnapEnabled ? snap : nullptr))
            return;

        const XMMATRIX desiredWorld = XMLoadFloat4x4(&edited);
        const XMMATRIX desiredModel = desiredWorld *
            XMMatrixInverse(nullptr, entityWorld);
        const XMMATRIX correction = desiredModel *
            XMMatrixInverse(nullptr, uncorrectedModel);
        XMVECTOR scale;
        XMVECTOR quaternion;
        XMVECTOR translation;
        if (!XMMatrixDecompose(&scale, &quaternion, &translation, correction))
            return;
        XMFLOAT3 position{};
        XMFLOAT4 q{};
        XMStoreFloat3(&position, translation);
        XMStoreFloat4(&q, quaternion);
        const float pitch = std::asin(std::clamp(
            2.0f * (q.w * q.x - q.y * q.z), -1.0f, 1.0f));
        const float yaw = std::atan2(
            2.0f * (q.x * q.z + q.w * q.y),
            1.0f - 2.0f * (q.x * q.x + q.y * q.y));
        const float roll = std::atan2(
            2.0f * (q.x * q.y + q.w * q.z),
            1.0f - 2.0f * (q.x * q.x + q.z * q.z));
        DirectX::XMFLOAT3 rotation = {
            XMConvertToDegrees(pitch),
            XMConvertToDegrees(yaw),
            XMConvertToDegrees(roll) };
        if (IsAnimationElbowPoleControl(m_animationStudioBone))
            rotation = {};
        const float keyTime = editUpperLayer
            ? poseAnimation->socketPreviewUpperTime
            : poseAnimation->socketPreviewTime;
        AnimationPoseKey& key = EnsurePoseKey(*poseAnimation,
            clip->name, m_animationStudioBone, -1, keyTime,
            position, rotation);
        key.position = position;
        key.rotation = rotation;
        poseAnimation->socketPreviewPlaying = false;
        if (editUpperLayer)
            poseAnimation->upperBodyTime = keyTime;
        else
            poseAnimation->currentTime = keyTime;
        return;
    }

    const Entity weaponEntity =
        WeaponSystem::GetEquippedWeaponEntity(registry, studioTarget);
    auto* weapon = registry.GetComponent<WeaponComponent>(weaponEntity);
    const bool firstPerson = !npcTarget && m_cameraController->IsFirstPerson();
    Entity poseEntity = studioTarget;
    if (firstPerson)
    {
        poseEntity = INVALID_ENTITY;
        registry.ForEach<FirstPersonViewComponent>(
            [&](Entity entity, FirstPersonViewComponent& viewModel)
            {
                if (viewModel.owner == studioTarget && viewModel.followCamera)
                    poseEntity = entity;
            });
    }
    auto* animation = registry.GetComponent<AnimationComponent>(poseEntity);
    const auto* mesh = registry.GetComponent<MeshRendererComponent>(poseEntity);
    const auto* transform = registry.GetComponent<TransformComponent>(poseEntity);
    if (!weapon || !animation || !mesh || !mesh->model || !transform)
        return;

    using namespace DirectX;
    XMMATRIX hand;
    if (!mesh->model->TryGetBoneModelTransform(
            *animation, weapon->rightHandBone, hand))
        return;
    const XMMATRIX handWorld = hand * transform->GetWorldMatrix();
    WeaponGripTransform& activeGrip = firstPerson
        ? weapon->firstPersonGrip : weapon->worldGrip;
    const XMMATRIX weaponWorld = WeaponIKSystem::GripMatrix(activeGrip) * handWorld;
    const auto& wrist = firstPerson ? weapon->firstPersonLeftHandGrip
        : weapon->leftHandGrip;
    const auto& wristRotation = firstPerson ? weapon->firstPersonLeftHandRotation
        : weapon->leftHandRotation;
    const auto& pivot = activeGrip.pivot;
    const XMMATRIX gripPivotWorld = XMMatrixTranslation(
        pivot.x, pivot.y, pivot.z) * weaponWorld;
    const XMMATRIX wristWorld = XMMatrixRotationRollPitchYaw(
        wristRotation.x, wristRotation.y, wristRotation.z) *
        XMMatrixTranslation(wrist.x, wrist.y, wrist.z) * weaponWorld;
    const auto& muzzle = weapon->muzzlePosition;
    const XMMATRIX muzzleWorld = XMMatrixTranslation(
        muzzle.x, muzzle.y, muzzle.z) * weaponWorld;
    const XMMATRIX markers[] = { gripPivotWorld, wristWorld, muzzleWorld };
    const char* labels[] = { "Grip", "Left wrist", "Muzzle" };
    const ImU32 colors[] = {
        IM_COL32(40, 175, 255, 255),
        IM_COL32(70, 230, 120, 255),
        IM_COL32(255, 180, 50, 255) };

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const XMMATRIX viewProjection = view * projection;
    for (int i = 0; i < 3; ++i)
    {
        const XMVECTOR point = XMVector3Transform(markers[i].r[3], viewProjection);
        const float w = XMVectorGetW(point);
        if (w <= 0.0001f) continue;
        const float x = XMVectorGetX(point) / w;
        const float y = XMVectorGetY(point) / w;
        const float z = XMVectorGetZ(point) / w;
        if (z < 0.0f || z > 1.0f || std::abs(x) > 1.0f || std::abs(y) > 1.0f)
            continue;
        const ImVec2 screen{ (x * 0.5f + 0.5f) * display.x,
            (1.0f - (y * 0.5f + 0.5f)) * display.y };
        draw->AddCircleFilled(screen, i == m_socketStudioSocket ? 7.0f : 4.0f,
            colors[i]);
        draw->AddText(ImVec2(screen.x + 10.0f, screen.y - 7.0f),
            colors[i], labels[i]);
    }

    if (!m_socketStudioUiCursorActive)
        return;

    XMMATRIX socketWorld = markers[m_socketStudioSocket];
    XMFLOAT4X4 edited{};
    XMStoreFloat4x4(&edited, socketWorld);
    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
    ImGuizmo::SetRect(0.0f, 0.0f, display.x, display.y);
    const ImGuizmo::OPERATION operation =
        m_socketStudioSocket == 2 || m_socketStudioGizmoMode == 0
        ? ImGuizmo::TRANSLATE : ImGuizmo::ROTATE;
    if (!ImGuizmo::Manipulate(reinterpret_cast<const float*>(&view),
            reinterpret_cast<const float*>(&projection), operation,
            m_socketStudioSocket == 0 && operation == ImGuizmo::ROTATE
                ? ImGuizmo::LOCAL : ImGuizmo::WORLD, &edited._11))
        return;

    const XMMATRIX changedWorld = XMLoadFloat4x4(&edited);
    const XMMATRIX parentWorld = m_socketStudioSocket == 0
        ? handWorld : weaponWorld;
    const XMMATRIX local = changedWorld * XMMatrixInverse(nullptr, parentWorld);
    XMVECTOR scale, rotation, translation;
    if (!XMMatrixDecompose(&scale, &rotation, &translation, local))
        return;
    XMFLOAT3 position{};
    XMStoreFloat3(&position, translation);
    if (m_socketStudioSocket == 2)
    {
        weapon->muzzlePosition = position;
        return;
    }

    XMFLOAT4 q{};
    XMStoreFloat4(&q, rotation);
    const float pitch = std::asin(std::clamp(
        2.0f * (q.w * q.x - q.y * q.z), -1.0f, 1.0f));
    const float yaw = std::atan2(
        2.0f * (q.x * q.z + q.w * q.y),
        1.0f - 2.0f * (q.x * q.x + q.y * q.y));
    const float roll = std::atan2(
        2.0f * (q.x * q.y + q.w * q.z),
        1.0f - 2.0f * (q.x * q.x + q.z * q.z));
    if (m_socketStudioSocket == 0)
    {
        activeGrip.position = { position.x - pivot.x,
            position.y - pivot.y, position.z - pivot.z };
        if (operation == ImGuizmo::ROTATE)
            activeGrip.rotation = { pitch, yaw, roll };

        // При настройке оружия в третьем лице оставляем цель левой кисти
        // на прежнем месте и пересчитываем только её локальные координаты.
        if (!firstPerson && m_socketStudioKeepLeftHandInPlace &&
            weapon->enableLeftHandIK)
        {
            const XMMATRIX changedWeaponWorld =
                WeaponIKSystem::GripMatrix(activeGrip) * handWorld;
            SetLeftWristTargetFromMatrix(
                *weapon,
                wristWorld * XMMatrixInverse(nullptr, changedWeaponWorld),
                false);
        }
    }
    else
    {
        auto& targetPosition = firstPerson ? weapon->firstPersonLeftHandGrip
            : weapon->leftHandGrip;
        auto& targetRotation = firstPerson ? weapon->firstPersonLeftHandRotation
            : weapon->leftHandRotation;
        targetPosition = position;
        if (operation == ImGuizmo::ROTATE)
            targetRotation = { pitch, yaw, roll };
    }
}

void EditorUI::BeginMainMenuBar(SceneManager& sceneManager)
{
}

//=============================================================================
// RenderSceneOutliner Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦, Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
//=============================================================================
void EditorUI::RenderSceneOutliner(SceneManager& sceneManager)
{
    ImGui::Spacing();
    SectionHeader("Scene Hierarchy");
    ImGui::Spacing();

    ImGui::SetNextItemWidth(-42.0f);
    ImGui::InputTextWithHint(
        "##OutlinerSearch", "Search name, type or entity id...",
        m_outlinerSearch, sizeof(m_outlinerSearch));
    ImGui::SameLine();
    ImGui::BeginDisabled(m_outlinerSearch[0] == '\0');
    if (ImGui::Button("X##ClearOutlinerSearch"))
        m_outlinerSearch[0] = '\0';
    ImGui::EndDisabled();
    ImGui::Checkbox("Show inactive", &m_showInactiveEntities);

    int visibleEntityCount = 0;

    const float avail = ImGui::GetContentRegionAvail().y - std::floor(4.0f * ScaleY());
    ImGui::BeginChild("##OutlinerList", ImVec2(-1, avail), false);

    auto& registry = sceneManager.GetECSRegistry();

    auto drawSelectableEntity = [&](const char* label, Entity entity, SelectedType type)
    {
        if (!ContainsCaseInsensitive(label, m_outlinerSearch))
            return;

        const bool inactive = type != SelectedType::Sprite &&
            !sceneManager.IsObjectActive(entity);
        if (inactive && !m_showInactiveEntities)
            return;

        ++visibleEntityCount;
        const bool selected = (m_selectedType == type && m_selectedEntity == entity);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Header, COL_SELECTED);
        if (inactive) ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
        if (ImGui::Selectable(label, selected))
        {
            SetSelection(type, entity);
            // Keep the NPC Studio inspector synchronized with the ordinary
            // scene outliner. This does not require a squad component.
            if (registry.HasComponent<NPCComponent>(entity))
                NpcSimulationStudio::inspectedNpc = entity;
        }
        if (inactive) ImGui::PopStyleColor();
        if (selected) ImGui::PopStyleColor();
    };

    std::vector<Entity> colliderEntities;

    auto isColliderEntity = [&](Entity entity)
    {
        auto* name = registry.GetComponent<NameComponent>(entity);
        if (!name) return false;

        const std::string& value = name->name;
        return value.rfind("COL_", 0) == 0 ||
            value.rfind("SHIP_COL_", 0) == 0 ||
            value.rfind("Collider", 0) == 0 ||
            value.rfind("Collider:", 0) == 0;
    };

    auto addColliderEntity = [&](Entity entity)
    {
        if (std::find(colliderEntities.begin(), colliderEntities.end(), entity) == colliderEntities.end())
            colliderEntities.push_back(entity);
    };

    std::map<std::string, std::vector<Entity>> panelsByRoom;
    std::vector<Entity> ungroupedPanels;
    auto& panelPool = registry.GetComponents<HullPanelComponent>();
    for (auto& [entity, panel] : panelPool)
    {
        if (!m_showInactiveEntities && !sceneManager.IsObjectActive(entity)) continue;

        if (!panel.roomName.empty())
            panelsByRoom[panel.roomName].push_back(entity);
        else
            ungroupedPanels.push_back(entity);
    }

    // -- Mesh objects, except hull panels. Panels are rendered under rooms below.
    auto& meshPool = registry.GetComponents<MeshRendererComponent>();
    for (auto& [entity, mesh] : meshPool)
    {
        if (!m_showInactiveEntities && !sceneManager.IsObjectActive(entity)) continue;
        if (registry.HasComponent<HullPanelComponent>(entity)) continue;
        if (isColliderEntity(entity))
        {
            addColliderEntity(entity);
            continue;
        }

        auto* light = registry.GetComponent<LightComponent>(entity);

        char label[96];
        if (light)
        {
            snprintf(label, sizeof(label), "  Light [%u]", entity);
        }
        else if (registry.HasComponent<NPCComponent>(entity))
        {
            const std::string debugName = EditorEntityDebugLabel(registry, entity);
            snprintf(label, sizeof(label), "  NPC: %s", debugName.c_str());
        }
        else if (auto* name = registry.GetComponent<NameComponent>(entity))
        {
            snprintf(label, sizeof(label), "  Object: %s [%u]", name->name.c_str(), entity);
        }
        else
        {
            snprintf(label, sizeof(label), "  Object [%u]", entity);
        }

        drawSelectableEntity(label, entity, light ? SelectedType::Light : SelectedType::Object);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // -- Logical rooms with grouped hull panels.
    auto& roomPool = registry.GetComponents<RoomComponent>();
    for (auto& [roomEntity, room] : roomPool)
    {
        if (!m_showInactiveEntities && !sceneManager.IsObjectActive(roomEntity)) continue;
        if (registry.HasComponent<MeshRendererComponent>(roomEntity)) continue;

        auto panelIt = panelsByRoom.find(room.roomName);
        const int panelCount = panelIt != panelsByRoom.end()
            ? static_cast<int>(panelIt->second.size())
            : 0;

        char roomLabel[128];
        snprintf(roomLabel, sizeof(roomLabel), "Room: %s [%u]  Panels: %d", room.roomName.c_str(), roomEntity, panelCount);

        bool roomOrPanelMatches = ContainsCaseInsensitive(roomLabel, m_outlinerSearch);
        if (!roomOrPanelMatches && panelIt != panelsByRoom.end())
        {
            for (Entity panelEntity : panelIt->second)
            {
                if (auto* panel = registry.GetComponent<HullPanelComponent>(panelEntity))
                {
                    if (ContainsCaseInsensitive(panel->panelName.c_str(), m_outlinerSearch))
                    {
                        roomOrPanelMatches = true;
                        break;
                    }
                }
            }
        }
        if (!roomOrPanelMatches)
            continue;

        ++visibleEntityCount;

        ImGuiTreeNodeFlags roomFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (m_selectedType == SelectedType::Object && m_selectedEntity == roomEntity)
            roomFlags |= ImGuiTreeNodeFlags_Selected;
        if (panelCount == 0)
            roomFlags |= ImGuiTreeNodeFlags_Leaf;

        const bool roomOpen = ImGui::TreeNodeEx((void*)(uintptr_t)roomEntity, roomFlags, "%s", roomLabel);
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            SetSelection(SelectedType::Object, roomEntity);

        if (roomOpen)
        {
            if (panelIt != panelsByRoom.end())
            {
                char panelGroupLabel[64];
                snprintf(panelGroupLabel, sizeof(panelGroupLabel), "Panels (%d)", panelCount);
                if (ImGui::TreeNode(panelGroupLabel))
                {
                    for (Entity panelEntity : panelIt->second)
                    {
                        auto* panel = registry.GetComponent<HullPanelComponent>(panelEntity);
                        if (!panel) continue;

                        char panelLabel[160];
                        snprintf(panelLabel, sizeof(panelLabel), "  %s [%u]", panel->panelName.c_str(), panelEntity);
                        drawSelectableEntity(panelLabel, panelEntity, SelectedType::Object);
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }
    }

    if (!ungroupedPanels.empty())
    {
        ImGui::Spacing();
        if (ImGui::TreeNode("Ungrouped Panels"))
        {
            for (Entity panelEntity : ungroupedPanels)
            {
                auto* panel = registry.GetComponent<HullPanelComponent>(panelEntity);
                if (!panel) continue;

                char panelLabel[160];
                snprintf(panelLabel, sizeof(panelLabel), "  %s [%u]", panel->panelName.c_str(), panelEntity);
                drawSelectableEntity(panelLabel, panelEntity, SelectedType::Object);
            }
            ImGui::TreePop();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // -- Areas ----------------------------------------------------------------
    auto& areaPool = registry.GetComponents<WorldAreaComponent>();
    for (auto& [entity, area] : areaPool)
    {
        if (!m_showInactiveEntities && !sceneManager.IsObjectActive(entity)) continue;
        if (registry.HasComponent<MeshRendererComponent>(entity)) continue;

        const char* typeName = WorldAreaTypeLabel(area.type);

        char label[64];
        snprintf(label, sizeof(label), "  Area: %s [%u]", typeName, entity);
        drawSelectableEntity(label, entity, SelectedType::Object);
    }

    // -- Collision-only entities ---------------------------------------------
    auto& rbPool = registry.GetComponents<RigidBodyComponent>();
    for (auto& [entity, rb] : rbPool)
    {
        if (!m_showInactiveEntities && !sceneManager.IsObjectActive(entity)) continue;
        if (registry.HasComponent<WorldAreaComponent>(entity)) continue;
        if (registry.HasComponent<RoomComponent>(entity)) continue;

        if (!registry.HasComponent<MeshRendererComponent>(entity) || isColliderEntity(entity))
            addColliderEntity(entity);
    }

    if (!colliderEntities.empty())
    {
        ImGui::Spacing();
        char colliderGroupLabel[64];
        snprintf(colliderGroupLabel, sizeof(colliderGroupLabel), "Colliders (%d)", static_cast<int>(colliderEntities.size()));
        if (ImGui::TreeNode(colliderGroupLabel))
        {
            for (Entity colliderEntity : colliderEntities)
            {
                std::string name = EditorEntityDebugLabel(registry, colliderEntity);
                char label[192];
                snprintf(label, sizeof(label), "  Collider: %s", name.c_str());
                drawSelectableEntity(label, colliderEntity, SelectedType::Object);
            }
            ImGui::TreePop();
        }
    }

    // -- Logical/gameplay entities without a render mesh ---------------------
    // Empty authoring points, NPC logic entities and standalone lights used to
    // disappear from the hierarchy because the old outliner only iterated the
    // mesh pool. Keep them editable through the normal no-code workflow.
    std::vector<Entity> logicalEntities;
    auto& transformPool = registry.GetComponents<TransformComponent>();
    for (auto& [entity, transform] : transformPool)
    {
        if (!m_showInactiveEntities && !sceneManager.IsObjectActive(entity)) continue;
        if (registry.HasComponent<MeshRendererComponent>(entity)) continue;
        if (registry.HasComponent<RoomComponent>(entity)) continue;
        if (registry.HasComponent<WorldAreaComponent>(entity)) continue;
        if (registry.HasComponent<RigidBodyComponent>(entity)) continue;
        logicalEntities.push_back(entity);
    }
    std::sort(logicalEntities.begin(), logicalEntities.end());

    if (!logicalEntities.empty())
    {
        ImGui::Spacing();
        if (ImGui::TreeNodeEx(
            "Gameplay Entities", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
        {
            for (Entity entity : logicalEntities)
            {
                const std::string debugName = EditorEntityDebugLabel(registry, entity);
                char label[224];
                SelectedType type = SelectedType::Object;
                if (registry.HasComponent<LightComponent>(entity))
                {
                    type = SelectedType::Light;
                    snprintf(label, sizeof(label), "  Light: %s", debugName.c_str());
                }
                else if (registry.HasComponent<TriggerZoneComponent>(entity))
                {
                    snprintf(label, sizeof(label), "  Trigger: %s", debugName.c_str());
                }
                else if (registry.HasComponent<NPCComponent>(entity))
                {
                    snprintf(label, sizeof(label), "  NPC: %s", debugName.c_str());
                }
                else if (registry.HasComponent<InteractionComponent>(entity))
                {
                    snprintf(label, sizeof(label), "  Interaction: %s", debugName.c_str());
                }
                else
                {
                    snprintf(label, sizeof(label), "  Entity: %s", debugName.c_str());
                }
                drawSelectableEntity(label, entity, type);
            }
            ImGui::TreePop();
        }
    }

    ImGui::Spacing();

    for (int i = 0; i < sceneManager.GetSpriteCount(); ++i)
    {
        char label[64];
        snprintf(label, sizeof(label), "  Sprite #%d", i);
        drawSelectableEntity(label, static_cast<Entity>(i), SelectedType::Sprite);
    }

    if (visibleEntityCount == 0)
    {
        ImGui::Spacing();
        ImGui::TextDisabled("No entities match the current filter.");
    }

    ImGui::EndChild();
}
//=============================================================================
void EditorUI::RenderDetailsPanel(SceneManager& sceneManager)
{
    ImGui::Begin("Details", nullptr, ImGuiWindowFlags_NoCollapse);

    if (m_selectedType == SelectedType::None)
    {
        ImGui::TextColored(COL_ACCENT, "Nothing selected");
        ImGui::TextWrapped(
            "Select an item in Scene Hierarchy or use + Create in the toolbar. "
            "Entity properties and components will appear here.");
        ImGui::End();
        return;
    }

    if (m_selectedType != SelectedType::Sprite &&
        (m_selectedEntity == INVALID_ENTITY ||
            !sceneManager.GetECSRegistry().IsAlive(m_selectedEntity)))
    {
        ImGui::TextColored(COL_RED, "The selected entity no longer exists.");
        m_selectedType = SelectedType::None;
        m_selectedEntity = INVALID_ENTITY;
        m_selectedLegacyIndex = -1;
        ImGui::End();
        return;
    }

    switch (m_selectedType)
    {
    case SelectedType::Object:
        if (m_selectedEntity != INVALID_ENTITY)
            RenderObjectProperties(sceneManager, m_selectedEntity);
        break;

    case SelectedType::Light:
        if (m_selectedEntity != INVALID_ENTITY)
        {
            RenderEntityIdentity(sceneManager, m_selectedEntity);
            RenderLightPropertiesByEntity(sceneManager, m_selectedEntity);
        }
        break;

    case SelectedType::Sprite:
        if (WorldSpriteClass* sprite = sceneManager.GetSprite(m_selectedLegacyIndex))
        {
            RenderSpriteProperties(sprite);
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.30f, 0.30f, 1.0f));
            if (ImGui::Button("Delete Sprite", ImVec2(-1.0f, 0.0f)))
                RequestDeleteSelection();
            ImGui::PopStyleColor(2);
        }
        else
        {
            ImGui::TextColored(COL_RED, "Sprite not found");
        }
        break;
    }

    ImGui::End();
}


//=============================================================================
// RenderObjectProperties Р С—РЎвЂ”Р вЂ¦ Transform + Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
//=============================================================================
void EditorUI::RenderObjectProperties(SceneManager& sceneManager, Entity entity)
{
    auto& registry = sceneManager.GetECSRegistry();
    RenderEntityIdentity(sceneManager, entity);
    auto* transform = sceneManager.GetObjectTransform(entity);
    if (!transform)
    {
        if (registry.HasComponent<WorldAreaComponent>(entity))
        {
            RenderComponentManager(sceneManager, entity);

            auto* combat = registry.GetComponent<NPCCombatComponent>(entity);


            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button("Remove Area", ImVec2(-1, 0)))
                RequestDeleteSelection();
            ImGui::PopStyleColor(2);
            return;
        }

        ImGui::TextColored(COL_RED, "Entity not found");
        return;
    }

    if (registry.HasComponent<SubmarineHullConfig>(entity))
    {
        auto* cfg = registry.GetComponent<SubmarineHullConfig>(entity);

        if (ImGui::CollapsingHeader("? Submarine Hull", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat3("Extents (m)", &cfg->hullExtents.x, 0.1f, 0.1f, 200.0f);
            ImGui::DragFloat("Wall Thickness (m)", &cfg->wallThickness, 0.05f, 0.1f, 5.0f);
            ImGui::Checkbox("Kinematic", &cfg->isKinematic);
            ImGui::Checkbox("Show Debug Hull", &cfg->showDebugHull);

            ImGui::Spacing();
            if (ImGui::Button("?? Build Hull Physics", { -1, 0 }))
           

            ImGui::Spacing();
            if (ImGui::Button("? Add SubmarineHullConfig", { -1, 0 }))
            { /* Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ */
            }
        }
    }
    else
    {
        if (ImGui::Button("? Add Submarine Hull Config"))
        {
            registry.AddComponent<SubmarineHullConfig>(entity, SubmarineHullConfig{});
            if (!registry.HasComponent<SubmarineComponent>(entity))
                registry.AddComponent<SubmarineComponent>(entity, SubmarineComponent{
                    .submarineRoot = entity
                    });
        }
    }
    if (auto* hullCfg = sceneManager.GetECSRegistry().GetComponent<SubmarineHullConfig>(entity))
    {
        if (ImGui::CollapsingHeader("? Hull Physics Config", ImGuiTreeNodeFlags_DefaultOpen))
        {
            bool changed = false;
            changed |= ImGui::DragFloat3("Hull Extents (m)",
                &hullCfg->hullExtents.x, 0.1f, 0.1f, 100.0f);
            changed |= ImGui::DragFloat("Wall Thickness (m)",
                &hullCfg->wallThickness, 0.05f, 0.1f, 5.0f);
            changed |= ImGui::Checkbox("Kinematic", &hullCfg->isKinematic);
            changed |= ImGui::Checkbox("Show Debug Hull", &hullCfg->showDebugHull);

            if (changed)
            {
                // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "? Press 'Rebuild' to apply");
            }

          
        }
    }

    // -- Transform -------------------------------------------------------------
    SectionHeader("Transform");
    ImGui::Spacing();

    float pos[3] = { transform->position.x, transform->position.y, transform->position.z };
    ImGui::TextColored(COL_TEXT_DIM, " Position");
    ImGui::SameLine();
    const bool resetPosition = ImGui::SmallButton("Reset##Position");
    if (resetPosition)
        pos[0] = pos[1] = pos[2] = 0.0f;
    if (DragFloat3Colored("##ObjPos", pos, 0.1f) || resetPosition)
    {
        if (m_transformSnapEnabled)
        {
            pos[0] = SnapEditorValue(pos[0], m_positionSnap);
            pos[1] = SnapEditorValue(pos[1], m_positionSnap);
            pos[2] = SnapEditorValue(pos[2], m_positionSnap);
        }
        transform->position = { pos[0], pos[1], pos[2] };
    }

    ImGui::Spacing();

    float rotDegrees[3] = {
        XMConvertToDegrees(transform->rotation.x),
        XMConvertToDegrees(transform->rotation.y),
        XMConvertToDegrees(transform->rotation.z) };
    ImGui::TextColored(COL_TEXT_DIM, " Rotation (degrees)");
    ImGui::SameLine();
    const bool resetRotation = ImGui::SmallButton("Reset##Rotation");
    if (resetRotation)
        rotDegrees[0] = rotDegrees[1] = rotDegrees[2] = 0.0f;
    if (DragFloat3Colored("##ObjRot", rotDegrees, 0.5f) || resetRotation)
    {
        if (m_transformSnapEnabled)
        {
            rotDegrees[0] = SnapEditorValue(rotDegrees[0], m_rotationSnapDegrees);
            rotDegrees[1] = SnapEditorValue(rotDegrees[1], m_rotationSnapDegrees);
            rotDegrees[2] = SnapEditorValue(rotDegrees[2], m_rotationSnapDegrees);
        }
        transform->rotation = {
            XMConvertToRadians(rotDegrees[0]),
            XMConvertToRadians(rotDegrees[1]),
            XMConvertToRadians(rotDegrees[2]) };
    }

    ImGui::Spacing();

    float scl[3] = { transform->scale.x, transform->scale.y, transform->scale.z };
    ImGui::TextColored(COL_TEXT_DIM, " Scale");
    ImGui::SameLine();
    const bool resetScale = ImGui::SmallButton("Reset##Scale");
    if (resetScale)
        scl[0] = scl[1] = scl[2] = 1.0f;
    if (DragFloat3Colored("##ObjScl", scl, 0.01f) || resetScale)
    {
        if (m_transformSnapEnabled)
        {
            scl[0] = SnapEditorValue(scl[0], m_scaleSnap);
            scl[1] = SnapEditorValue(scl[1], m_scaleSnap);
            scl[2] = SnapEditorValue(scl[2], m_scaleSnap);
        }
        transform->scale = { scl[0], scl[1], scl[2] };
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ --------------------------------------------------
    RenderComponentNavigation(sceneManager, entity);
    const float componentPanelHeight = (std::max)(
        180.0f, ImGui::GetContentRegionAvail().y - 48.0f);
    if (ImGui::BeginChild("ComponentInspectorScroll", ImVec2(0.0f, componentPanelHeight),
        ImGuiChildFlags_Borders))
    {
        RenderComponentManager(sceneManager, entity);
    }
    ImGui::EndChild();



    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ --------------------------------------------------------
    ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    if (ImGui::Button("Remove Object", ImVec2(-1, 0)))
        RequestDeleteSelection();
    ImGui::PopStyleColor(2);
}

//=============================================================================
// Быстрая навигация по компонентам выбранной сущности. Кнопка запоминает
// секцию, а RenderComponentManager раскрывает её и прокручивает инспектор.
//=============================================================================
void EditorUI::RenderComponentNavigation(SceneManager& sceneManager, Entity entity)
{
    auto& registry = sceneManager.GetECSRegistry();
    ImGui::SeparatorText("Components - quick access");

    if (ImGui::BeginChild("ComponentQuickAccess", ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY))
    {
        const float rightEdge = ImGui::GetWindowPos().x +
            ImGui::GetWindowContentRegionMax().x;
        bool first = true;
        auto addButton = [&](bool present, const char* label, const char* section)
        {
            if (!present) return;
            const float width = ImGui::CalcTextSize(label).x +
                ImGui::GetStyle().FramePadding.x * 2.0f;
            if (!first)
            {
                const float nextRight = ImGui::GetItemRectMax().x +
                    ImGui::GetStyle().ItemSpacing.x + width;
                if (nextRight <= rightEdge)
                    ImGui::SameLine();
            }
            ImGui::PushID(section);
            if (ImGui::SmallButton(label))
            {
                m_componentJumpEntity = entity;
                m_componentJumpTarget = section;
            }
            ImGui::PopID();
            first = false;
        };

        const bool hasHealth =
            registry.HasComponent<PlayerComponent>(entity) ||
            registry.HasComponent<NPCComponent>(entity) ||
            registry.HasComponent<DamageableComponent>(entity) ||
            registry.HasComponent<HullPanelComponent>(entity) ||
            registry.HasComponent<PressureHullComponent>(entity);
        addButton(hasHealth, "Health", "health");
        addButton(registry.HasComponent<NPCComponent>(entity), "NPC AI", "npc");
        addButton(registry.HasComponent<NpcCoverPointComponent>(entity), "Cover", "cover");
        addButton(registry.HasComponent<NpcPatrolPointComponent>(entity), "Patrol", "patrol");
        addButton(registry.HasComponent<AnimationComponent>(entity), "Animation", "animation");
        addButton(registry.HasComponent<FactionComponent>(entity), "Faction", "faction");
        addButton(registry.HasComponent<CrewMemberComponent>(entity), "Crew", "crew");
        addButton(registry.HasComponent<CharacterController>(entity), "Character Physics", "character_physics");
        addButton(registry.HasComponent<TriggerZoneComponent>(entity), "Trigger", "trigger");
        addButton(registry.HasComponent<TriggerActionComponent>(entity), "Trigger Action", "trigger_action");
        addButton(registry.HasComponent<RoomComponent>(entity), "Room", "room");
        addButton(registry.HasComponent<NavMeshSurfaceComponent>(entity), "NavMesh", "navmesh");
        addButton(registry.HasComponent<WorldAreaComponent>(entity), "World Area", "world_area");
        addButton(registry.HasComponent<HullPanelComponent>(entity), "Hull Panel", "hull_panel");
        addButton(registry.HasComponent<WeaponComponent>(entity), "Weapon", "weapon");
        addButton(registry.HasComponent<WeaponPickupComponent>(entity), "Pickup", "pickup");
        addButton(registry.HasComponent<InventoryComponent>(entity), "Inventory", "inventory");
        addButton(registry.HasComponent<InteractionComponent>(entity), "Interaction", "interaction");
        addButton(registry.HasComponent<DoorComponent>(entity), "Door", "door");
        addButton(registry.HasComponent<PlayerGameTag>(entity), "Player", "player");
        addButton(registry.HasComponent<FirstPersonViewComponent>(entity), "First Person", "first_person");
        addButton(registry.HasComponent<BoneAttachmentComponent>(entity), "Bone Attachment", "bone_attachment");
        addButton(registry.HasComponent<LightComponent>(entity), "Light", "light");
        addButton(registry.HasComponent<DamageableComponent>(entity), "Damageable", "damageable");
        addButton(registry.HasComponent<CharacterVitalsComponent>(entity), "Character Vitals", "vitals");
        addButton(registry.HasComponent<SubmarineComponent>(entity), "Submarine", "submarine");
        addButton(registry.HasComponent<RigidBodyComponent>(entity), "RigidBody", "rigidbody");
        addButton(registry.HasComponent<BuoyancyComponent>(entity), "Buoyancy", "buoyancy");
    }
    ImGui::EndChild();
}

void EditorUI::RenderComponentManager(SceneManager& sceneManager, Entity entity)
{
    auto& registry = sceneManager.GetECSRegistry();

    auto wantsJump = [&](const char* section)
    {
        return m_componentJumpEntity == entity &&
            m_componentJumpTarget == section;
    };
    auto prepareJump = [&](const char* section)
    {
        const bool requested = wantsJump(section);
        if (requested)
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        return requested;
    };
    auto finishJump = [&](bool requested)
    {
        if (!requested) return;
        ImGui::SetScrollHereY(0.0f);
        m_componentJumpTarget.clear();
        m_componentJumpEntity = INVALID_ENTITY;
    };

    const bool hasHealth =
        registry.HasComponent<CharacterVitalsComponent>(entity) ||
        registry.HasComponent<DamageableComponent>(entity) ||
        registry.HasComponent<HullPanelComponent>(entity) ||
        registry.HasComponent<PressureHullComponent>(entity);
    if (hasHealth)
    {
        ImGui::SeparatorText("Health & Damage");
        finishJump(wantsJump("health"));
        bool invulnerable = registry.HasComponent<InvulnerableComponent>(entity);
        if (ImGui::Checkbox("Invulnerable##entity_health", &invulnerable))
        {
            if (invulnerable)
            {
                if (!registry.HasComponent<InvulnerableComponent>(entity))
                    registry.AddComponent<InvulnerableComponent>(
                        entity, InvulnerableComponent{});
            }
            else
            {
                registry.RemoveComponent<InvulnerableComponent>(entity);
            }
            if (invulnerable)
            {
                if (auto* vitals =
                    registry.GetComponent<CharacterVitalsComponent>(entity))
                    vitals->health = vitals->maxHealth;
            }
        }
        ImGui::TextDisabled("Blocks all damage for this entity; saved with the scene.");
        ImGui::Spacing();
    }

    if (auto* vitals = registry.GetComponent<CharacterVitalsComponent>(entity))
    {
        ImGui::SeparatorText("Character Vitals");
        finishJump(wantsJump("vitals"));

        ImGui::DragFloat("Health##character_vitals", &vitals->health,
            1.0f, 0.0f, vitals->maxHealth);
        ImGui::DragFloat("Max health##character_vitals", &vitals->maxHealth,
            1.0f, 0.0f, 10000.0f);



        ImGui::DragFloat("Oxygen##character_vitals", &vitals->oxygen,
            1.0f, 0.0f, vitals->maxOxygen);
        ImGui::DragFloat("Max oxygen##character_vitals", &vitals->maxOxygen,
            1.0f, 0.0f, 10000.0f);
        ImGui::DragFloat("Oxygen change/sec##character_vitals",
            &vitals->oxygenChangePerSecond, 0.25f, 0.0f, 1000.0f);
        ImGui::DragFloat("Suffocation damage/sec##character_vitals",
            &vitals->suffocationDamagePerSecond, 0.25f, 0.0f, 1000.0f);
        ImGui::Checkbox(
            "Takes suffocation damage##character_vitals",
            &vitals->takesSuffocationDamage
        );

        ImGui::DragFloat("Stamina##character_vitals", &vitals->stamina,
            1.0f, 0.0f, vitals->maxStamina);
        ImGui::DragFloat("Max stamina##character_vitals", &vitals->maxStamina,
            1.0f, 0.0f, 10000.0f);
        ImGui::DragFloat("Stamina drain/sec##character_vitals",
            &vitals->staminaDrainPerSecond, 0.25f, 0.0f, 1000.0f);
        ImGui::DragFloat("Stamina recovery/sec##character_vitals",
            &vitals->staminaRecoveryPerSecond, 0.25f, 0.0f, 1000.0f);
        ImGui::DragFloat("Sprint start threshold##character_vitals",
            &vitals->sprintStartThreshold, 0.25f, 0.0f, vitals->maxStamina);

        vitals->Normalize();

        if (const auto* movement = registry.GetComponent<CharacterMovement>(entity))
        {
            ImGui::SeparatorText("Stamina runtime");
            ImGui::Text("Moving: %s", movement->isMoving ? "true" : "false");
            ImGui::Text("Wants sprint: %s", movement->wantsRun ? "true" : "false");
            ImGui::Text("Running: %s", movement->isRunning ? "true" : "false");
            ImGui::Text("Exhausted: %s", vitals->staminaExhausted ? "true" : "false");
            ImGui::Text("Can start sprint: %s",
                (!vitals->staminaExhausted && vitals->stamina > 0.0f)
                    ? "true" : "false");
            ImGui::Text("Current speed: %.2f", movement->currentSpeed);
        }
        ImGui::Spacing();
    }

    if (auto* npc = registry.GetComponent<NPCComponent>(entity))
    {
        ImGui::SeparatorText("NPC Components");
        finishJump(wantsJump("npc"));
        auto* brain = registry.GetComponent<NpcBrainComponent>(entity);
        auto* movement = registry.GetComponent<NPCMovementComponent>(entity);
        auto* combat = registry.GetComponent<NPCCombatComponent>(entity);
        ImGui::Checkbox("Hostile##npc", &npc->isHostile);

        if (combat)
        {
            ImGui::SeparatorText("Combat");

            ImGui::DragFloat(
                "See range##npc_combat",
                &combat->seeRange,
                0.1f, 0.0f, 1000.0f);

            ImGui::DragFloat(
                "Field of view##npc_combat",
                &combat->fieldOfViewDegrees,
                1.0f, 0.0f, 360.0f, "%.1f deg");

            ImGui::DragFloat(
                "Close awareness##npc_combat",
                &combat->closeAwarenessRange,
                0.05f, 0.0f, 10.0f, "%.2f m");

            ImGui::DragFloat(
                "Attack range##npc_combat",
                &combat->attackRange,
                0.1f, 0.0f, 1000.0f);

            ImGui::DragFloat(
                "Attack cooldown##npc_combat",
                &combat->attackCooldown,
                0.05f, 0.0f, 60.0f);

            ImGui::DragFloat(
                "Lose target delay##npc_combat",
                &combat->loseTargetDelay,
                0.05f, 0.0f, 60.0f);
            ImGui::DragFloat("Chase resume margin##npc_combat",
                &combat->chaseResumeMargin, 0.05f, 0.0f, 5.0f, "%.2f m");
            ImGui::DragFloat("Guard enter delay##npc_combat",
                &combat->stanceEnterDelay, 0.05f, 0.0f, 5.0f, "%.2f s");
            ImGui::DragFloat("Prediction time##npc_combat",
                &combat->predictionSeconds, 0.05f, 0.0f, 3.0f, "%.2f s");
            ImGui::DragFloat("Shooting range##npc_combat",
                &combat->rangedAttackRange, 0.25f, 0.1f, 1000.0f, "%.1f m");
            ImGui::DragFloat("Aim delay##npc_combat",
                &combat->rangedAimDelay, 0.05f, 0.0f, 10.0f, "%.2f s");
            ImGui::DragFloat("Max prediction distance##npc_combat",
                &combat->maxPredictionDistance, 0.1f, 0.0f, 10.0f, "%.2f m");
            ImGui::DragFloat("Prediction travel timeout##npc_combat",
                &combat->predictionTravelTimeout, 0.1f, 0.0f, 30.0f, "%.1f s");
            ImGui::Checkbox("Wander until first contact##npc_combat",
                &combat->wanderUntilContact);
            ImGui::DragFloat("Contact wander radius##npc_combat",
                &combat->contactWanderRadius,
                0.25f, 1.0f, 100.0f, "%.1f m");

            ImGui::Text(
                "Target entity: %u",
                static_cast<unsigned>(combat->targetEntity));

            ImGui::Text(
                "Time since seen: %.2f",
                combat->timeSinceTargetSeen);
        }
        if (auto* modules =
            registry.GetComponent<NpcCombatModulesComponent>(entity))
        {
            if (ImGui::CollapsingHeader(
                "Combat modules##npc_combat_modules",
                ImGuiTreeNodeFlags_DefaultOpen))
            {
                const auto applyEditorPreset = [&](NpcCombatModulePreset preset)
                {
                    NpcCombatDebugCommandSystem::ClearForcedRuntime(
                        registry,
                        entity);
                    modules->ApplyPreset(preset);
                    // Details may be edited while NPC Studio is paused. Run one
                    // regular simulation tick so module shutdown/startup cleanup
                    // is applied exactly like a change made in Combat Lab.
                    NpcSimulationStudio::step = true;
                };
                const auto editorModuleCheckbox = [&](const char* label, bool* value)
                {
                    if (ImGui::Checkbox(label, value))
                        NpcSimulationStudio::step = true;
                };

                if (ImGui::Button("Minimal duel##npc_modules_minimal"))
                    applyEditorPreset(NpcCombatModulePreset::MinimalDuel);
                ImGui::SameLine();
                if (ImGui::Button("Cover + peek##npc_modules_cover"))
                    applyEditorPreset(NpcCombatModulePreset::CoverAndPeek);
                ImGui::SameLine();
                if (ImGui::Button("Full individual##npc_modules_full"))
                    applyEditorPreset(NpcCombatModulePreset::FullIndividual);

                editorModuleCheckbox("Perception##npc_module", &modules->perception);
                editorModuleCheckbox("Engagement##npc_module", &modules->engagement);
                editorModuleCheckbox("Pursuit##npc_module", &modules->pursuit);
                editorModuleCheckbox("Stationary fire##npc_module", &modules->stationaryFire);
                editorModuleCheckbox("Search##npc_module", &modules->search);
                editorModuleCheckbox("Automatic tactics##npc_module", &modules->automaticTactics);
                editorModuleCheckbox("Cover selection##npc_module", &modules->coverSelection);
                editorModuleCheckbox("Peek cycle##npc_module", &modules->peekCycle);
                editorModuleCheckbox("Cover reposition##npc_module", &modules->coverReposition);
                editorModuleCheckbox("Dynamic cover##npc_module", &modules->dynamicCover);
                editorModuleCheckbox("Moving fire##npc_module", &modules->movingFire);
                editorModuleCheckbox("Incoming-fire reaction##npc_module",
                    &modules->incomingFireReaction);
                editorModuleCheckbox("Squad tactics (parked)##npc_module",
                    &modules->squadTactics);
            }
        }
        if (auto* tactical = registry.GetComponent<NpcTacticalComponent>(entity))
        {
            ImGui::SeparatorText("Individual combat tactics");
            static const char* tacticalStyleNames[] =
            {
                "Auto",
                "Balanced",
                "Assault",
                "Defensive"
            };
            int selectedStyle = static_cast<int>(tactical->selectedStyle);
            if (ImGui::Combo("Tactical style##npc_tactical",
                    &selectedStyle,
                    tacticalStyleNames,
                    IM_ARRAYSIZE(tacticalStyleNames)))
            {
                tactical->selectedStyle =
                    static_cast<NpcTacticalStyle>(selectedStyle);
                // Ручной выбор должен примениться на следующем AI-кадре,
                // не дожидаясь завершения прошлого автоматического решения.
                tactical->tacticCommitTimer = 0.0f;
            }
            ImGui::DragFloat("Tactical aggression##npc_tactical",
                &tactical->tacticalAggression,
                0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat("Tactic commitment##npc_tactical",
                &tactical->tacticCommitDuration,
                0.25f, 0.0f, 30.0f, "%.1f s");
            ImGui::Text("Active style: %s",
                NpcTacticalStyleName(tactical->activeStyle));
            ImGui::TextWrapped("Tactic reason: %s",
                tactical->tacticReason.c_str());
            ImGui::DragFloat("Decision interval##npc_tactical", &tactical->decisionInterval,
                0.05f, 0.05f, 5.0f, "%.2f s");
            ImGui::DragFloat("Hide duration##npc_tactical", &tactical->hideDuration,
                0.05f, 0.1f, 10.0f, "%.2f s");
            ImGui::DragFloat("Expose duration##npc_tactical", &tactical->exposeDuration,
                0.05f, 0.1f, 5.0f, "%.2f s");
            ImGui::DragFloat("Peek move speed##npc_tactical", &tactical->peekMoveSpeed,
                0.1f, 0.1f, 15.0f, "%.1f m/s");
            ImGui::DragInt("Moving burst size##npc_tactical", &tactical->movingBurstSize,
                0.1f, 1, 20);
            ImGui::DragFloat("Moving burst pause##npc_tactical",
                &tactical->movingBurstPauseDuration,
                0.05f, 0.0f, 5.0f, "%.2f s");
            ImGui::DragInt("Failed peeks before relocate##npc_tactical",
                &tactical->failedPeeksBeforeRelocate,
                0.1f, 1, 10);
            ImGui::DragFloat("Minimum cover hold##npc_tactical",
                &tactical->minimumCoverHoldDuration,
                0.25f, 0.0f, 30.0f, "%.1f s");
            ImGui::DragFloat("Cover validation grace##npc_tactical",
                &tactical->coverValidationGraceDuration,
                0.05f, 0.0f, 3.0f, "%.2f s");
            ImGui::DragFloat("Incoming fire awareness##npc_tactical",
                &tactical->incomingFireAwarenessRange,
                0.5f, 0.0f, 100.0f, "%.1f m");
            ImGui::DragFloat("Preferred range##npc_tactical", &tactical->preferredRange,
                0.1f, 0.5f, 50.0f, "%.1f m");
            ImGui::Checkbox("Dynamic cover fallback##npc_tactical",
                &tactical->allowDynamicCover);
            ImGui::DragFloat("Dynamic cover radius##npc_tactical",
                &tactical->dynamicCoverSearchRadius,
                0.25f, 4.0f, 30.0f, "%.1f m");
            ImGui::DragInt("Dynamic cover samples##npc_tactical",
                &tactical->dynamicCoverSamples,
                1.0f, 12, 96);
            ImGui::DragFloat("Flank weight##npc_tactical", &tactical->flankWeight,
                0.1f, 0.0f, 20.0f);
            ImGui::Text("Reserved cover: %u",
                static_cast<unsigned>(tactical->reservedCover));
            ImGui::TextWrapped("Status: %s", tactical->status.c_str());
        }

        if (ImGui::CollapsingHeader("NPC AI", ImGuiTreeNodeFlags_DefaultOpen))
        {
            char name[128]{};
            char description[256]{};
            strncpy_s(name, npc->name.c_str(), _TRUNCATE);
            strncpy_s(description, npc->description.c_str(), _TRUNCATE);
            if (ImGui::InputText("Name##npc", name, sizeof(name))) npc->name = name;
            if (ImGui::InputTextMultiline("Description##npc", description, sizeof(description), ImVec2(-1.0f, 60.0f))) npc->description = description;
            ImGui::Checkbox("Hostile##npc", &npc->isHostile);

            if (auto* patrol = registry.GetComponent<NpcPatrolComponent>(entity))
            {
                ImGui::SeparatorText("Patrol route");
                ImGui::Checkbox("Enabled##npc_patrol", &patrol->enabled);
                ImGui::DragInt("Route ID##npc_patrol", &patrol->routeId, 1.0f, 0, 10000);
                ImGui::Checkbox("Loop##npc_patrol", &patrol->loop);
                ImGui::SameLine();
                ImGui::Checkbox("Ping-pong##npc_patrol", &patrol->pingPong);
                ImGui::Checkbox("Allow running##npc_patrol", &patrol->allowRunning);
                ImGui::DragFloat("Arrival distance##npc_patrol",
                    &patrol->arrivalDistance, 0.05f, 0.05f, 5.0f, "%.2f m");
                ImGui::DragFloat("Default wait##npc_patrol",
                    &patrol->defaultWaitTime, 0.1f, 0.0f, 60.0f, "%.1f s");
                ImGui::TextWrapped("Patrol: %s", patrol->status.c_str());
                ImGui::TextDisabled(
                    "Uses every enabled Patrol Point with the same Route ID, ordered by Order.");
            }

            if (brain)
            {
                // РџРѕР»РЅР°СЏ РІРµСЂСЃРёСЏ С‚РѕР№ Р¶Рµ РґРёР°РіРЅРѕСЃС‚РёРєРё РґРѕСЃС‚СѓРїРЅР° РІ РёРЅСЃРїРµРєС‚РѕСЂРµ Edit mode.
                // Runtime-РїРѕР»СЏ Р·РґРµСЃСЊ РїРѕРєР°Р·Р°РЅС‹ РЅР°РјРµСЂРµРЅРЅРѕ: СѓС‡РµР±РЅРѕ СѓРґРѕР±РЅРѕ РІРёРґРµС‚СЊ, РєР°Рє
                // РѕРґРЅР° РєРѕРјР°РЅРґР° РјРµРЅСЏРµС‚ state, РґРІРµСЂСЊ, РёРЅРґРµРєСЃ РјР°СЂС€СЂСѓС‚Р° Рё progress.
                ImGui::SeparatorText("AI Runtime Status");
                ImGui::Text("State: %s", NpcStateName(brain->state));
                ImGui::TextWrapped("Status: %s", brain->status.c_str());
                ImGui::Text("Command: %s", NpcCommandName(brain->command.type));
                ImGui::Text("Room: %s", EditorEntityDebugLabel(registry, brain->currentRoom).c_str());
                ImGui::Text("Active door: %s", EditorEntityDebugLabel(registry, brain->activeDoor).c_str());
                ImGui::Text("Route: %d / %d doors",
                    static_cast<int>(brain->doorRouteIndex),
                    static_cast<int>(brain->doorRoute.size()));
                const NpcNavMeshDebugInfo navInfo = NpcNavMeshSystem::GetDebugInfo(entity);
                ImGui::Text("NavMesh: %s, polygons: %d",
                    navInfo.compiledIn ? "enabled" : "disabled",
                    navInfo.polygonCount);
                ImGui::TextWrapped("Nav status: %s", navInfo.status.c_str());
                ImGui::Text("Local path: %d / %d",
                    navInfo.currentWaypoint,
                    navInfo.waypointCount);
                if (!brain->blockedReason.empty())
                    ImGui::TextColored(COL_RED, "Blocked: %s", brain->blockedReason.c_str());

                ImGui::Text("Repair target: %u", static_cast<unsigned>(brain->repairTarget));
                ImGui::ProgressBar(brain->repairScore, ImVec2(-1.0f, 0.0f), "Utility repair score");
                if (brain->state == NpcState::Repairing)
                    ImGui::ProgressBar(brain->repairProgress, ImVec2(-1.0f, 0.0f), "Repair progress");

                ImGui::Spacing();
                const bool debugWasEnabled = brain->debugControlEnabled;
                ImGui::Checkbox("Enable manual debug control##npc", &brain->debugControlEnabled);
                if (debugWasEnabled && !brain->debugControlEnabled)
                    NpcCommandSystem::ResumeAutomatic(*brain);

                ImGui::BeginDisabled(!brain->debugControlEnabled);
                if (ImGui::Button("Stop##npc", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 2.0f, 0.0f)))
                    NpcCommandSystem::Stop(*brain);
                ImGui::SameLine();
                if (ImGui::Button("Resume auto##npc", ImVec2(-1.0f, 0.0f)))
                    NpcCommandSystem::ResumeAutomatic(*brain);

                if (ImGui::Button("Wander##npc", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 2.0f, 0.0f)))
                    NpcCommandSystem::Wander(*brain);
                ImGui::SameLine();
                if (ImGui::Button("Force repath##npc", ImVec2(-1.0f, 0.0f)))
                {
                    NpcCommandSystem::ForceRepath(*brain);
                    NpcNavMeshSystem::InvalidatePath(entity);
                }

                if (ImGui::Button("Rebuild all NavMeshes##npc", ImVec2(-1.0f, 0.0f)))
                {
                    NpcNavMeshSystem::InvalidateAll();
                    NpcCommandSystem::ForceRepath(*brain);
                }

                ImGui::SeparatorText("Go to room");
                const std::string selectedRoomLabel =
                    EditorEntityDebugLabel(registry, brain->debugSelectedRoom);
                if (ImGui::BeginCombo("Room##npc_debug_room", selectedRoomLabel.c_str()))
                {
                    for (auto& [roomEntity, room] : registry.GetComponents<RoomComponent>())
                    {
                        const bool selected = brain->debugSelectedRoom == roomEntity;
                        const std::string label = room.roomName + " [" + std::to_string(roomEntity) + "]";
                        if (ImGui::Selectable(label.c_str(), selected))
                            brain->debugSelectedRoom = roomEntity;
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::BeginDisabled(brain->debugSelectedRoom == INVALID_ENTITY);
                if (ImGui::Button("Go to selected room##npc", ImVec2(-1.0f, 0.0f)))
                    NpcCommandSystem::MoveToRoom(*brain, brain->debugSelectedRoom);
                ImGui::EndDisabled();

                ImGui::SeparatorText("Go to world position");
                ImGui::DragFloat3("Target XYZ##npc", &brain->debugWorldTarget.x, 0.1f);
                if (ImGui::Button("Go to position##npc", ImVec2(-1.0f, 0.0f)))
                    NpcCommandSystem::MoveToPosition(*brain, brain->debugWorldTarget);

                ImGui::SeparatorText("Entity commands");
                ImGui::InputScalar("Target entity ID##npc", ImGuiDataType_U32, &brain->debugTargetEntity);
                if (ImGui::Button("Follow entity##npc", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 2.0f, 0.0f)))
                    NpcCommandSystem::FollowEntity(*brain, brain->debugTargetEntity);
                ImGui::SameLine();
                if (ImGui::Button("Repair entity##npc", ImVec2(-1.0f, 0.0f)))
                    NpcCommandSystem::RepairEntity(*brain, brain->debugTargetEntity);

                Entity playerEntity = INVALID_ENTITY;
                registry.ForEach<PlayerGameTag>([&](Entity candidate, PlayerGameTag&)
                    {
                        if (playerEntity == INVALID_ENTITY)
                            playerEntity = candidate;
                    });
                ImGui::BeginDisabled(playerEntity == INVALID_ENTITY);
                if (ImGui::Button("Follow player##npc", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 2.0f, 0.0f)))
                    NpcCommandSystem::FollowEntity(*brain, playerEntity);
                ImGui::EndDisabled();
                ImGui::SameLine();

                // РћР±С‹С‡РЅРѕ Р»СѓС‡С€Р°СЏ РїСЂРѕР±РѕРёРЅР° СѓР¶Рµ РІС‹Р±СЂР°РЅР° Utility AI. Р•СЃР»Рё automatic
                // target РµС‰С‘ РЅРµ СѓСЃРїРµР» РїРѕСЏРІРёС‚СЊСЃСЏ, РєРЅРѕРїРєР° РІСЃС‘ СЂР°РІРЅРѕ РѕСЃС‚Р°С‘С‚СЃСЏ РїРѕР»РµР·РЅРѕР№:
                // UI РЅР°С…РѕРґРёС‚ Р±Р»РёР¶Р°Р№С€СѓСЋ РѕС‚РєСЂС‹С‚СѓСЋ РїСЂРѕР±РѕРёРЅСѓ РїСЂРѕСЃС‚С‹Рј РїРµСЂРµР±РѕСЂРѕРј.
                Entity nearestBreach = brain->repairTarget;
                if (nearestBreach == INVALID_ENTITY)
                {
                    const auto* npcTransform = registry.GetComponent<TransformComponent>(entity);
                    float bestDistanceSq = (std::numeric_limits<float>::max)();
                    if (npcTransform)
                    {
                        for (auto& [breachEntity, breach] : registry.GetComponents<HullBreachComponent>())
                        {
                            if (breach.sealed)
                                continue;
                            const float dx = breach.worldPosition.x - npcTransform->position.x;
                            const float dy = breach.worldPosition.y - npcTransform->position.y;
                            const float dz = breach.worldPosition.z - npcTransform->position.z;
                            const float distanceSq = dx * dx + dy * dy + dz * dz;
                            if (distanceSq < bestDistanceSq)
                            {
                                bestDistanceSq = distanceSq;
                                nearestBreach = breachEntity;
                            }
                        }
                    }
                }
                ImGui::BeginDisabled(nearestBreach == INVALID_ENTITY);
                if (ImGui::Button("Repair nearest##npc", ImVec2(-1.0f, 0.0f)))
                    NpcCommandSystem::RepairEntity(*brain, nearestBreach);
                ImGui::EndDisabled();
                ImGui::EndDisabled();

                // Р­С‚Рё Р·РЅР°С‡РµРЅРёСЏ РјРµРЅСЏСЋС‚СЃСЏ РїСЂСЏРјРѕ РІ component Рё РЅР°С‡РёРЅР°СЋС‚ РґРµР№СЃС‚РІРѕРІР°С‚СЊ
                // РІ СЃР»РµРґСѓСЋС‰РµРј РєР°РґСЂРµ. Р­С‚Рѕ runtime tuning, Р° РЅРµ РѕС‚РґРµР»СЊРЅР°СЏ РєРѕРїРёСЏ UI.
                ImGui::SeparatorText("AI Tuning");
                ImGui::DragFloat("Detection range##npc", &brain->detectionRange, 0.1f, 0.1f, 1000.0f);
                ImGui::DragFloat("Repair distance##npc", &brain->repairDistance, 0.05f, 0.1f, 10.0f);
                ImGui::DragFloat("Repair duration##npc", &brain->repairDuration, 0.1f, 0.1f, 120.0f);
                ImGui::DragFloat("Repath interval##npc", &brain->repathInterval, 0.05f, 0.1f, 10.0f);
                ImGui::DragFloat("Stuck timeout##npc", &brain->stuckTimeout, 0.1f, 0.5f, 30.0f);
            }

            if (movement)
            {
                // РќР°СЃС‚СЂРѕР№РєРё Р¶РµР»Р°РµРјРѕРіРѕ РґРІРёР¶РµРЅРёСЏ. Р¤РёР·РёС‡РµСЃРєРёР№ СЂР°Р·РјРµСЂ capsule Рё
                // РїР°СЂР°РјРµС‚СЂС‹ Р°РЅРёРјР°С†РёРѕРЅРЅС‹С… РєР»РёРїРѕРІ РЅР°СЃС‚СЂР°РёРІР°СЋС‚СЃСЏ РІ РґСЂСѓРіРёС… РєРѕРјРїРѕРЅРµРЅС‚Р°С….
                ImGui::SeparatorText("Movement");
                ImGui::DragFloat("Speed##npc", &movement->speed, 0.1f, 0.0f, 100.0f);
                ImGui::DragFloat("Run speed##npc", &movement->runSpeed, 0.1f, 0.0f, 100.0f);
                ImGui::DragFloat("Run distance##npc", &movement->runWhenFartherThan, 0.1f, 0.0f, 100.0f);
                ImGui::DragFloat("Arrival distance##npc", &movement->arrivalDistance, 0.05f, 0.05f, 10.0f);
                ImGui::DragFloat("Door exit distance##npc", &movement->doorExitDistance, 0.05f, 0.5f, 5.0f);
                ImGui::TextWrapped("NPC passes this far beyond a door before turning to its final target.");
                ImGui::Checkbox("Allow running##npc", &movement->allowRunning);
                ImGui::Checkbox("Can fly##npc", &movement->canFly);
            }

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button("Remove NPC AI", ImVec2(-1.0f, 0.0f)))
            {
                registry.RemoveComponent<NPCComponent>(entity);
                registry.RemoveComponent<NpcBrainComponent>(entity);
                registry.RemoveComponent<NPCMovementComponent>(entity);
                registry.RemoveComponent<NpcPatrolComponent>(entity);
            }
            ImGui::PopStyleColor(2);
        }
    }
    if (auto* cover = registry.GetComponent<NpcCoverPointComponent>(entity))
    {
        ImGui::SeparatorText("NPC Cover Point");
        finishJump(wantsJump("cover"));
        ImGui::Checkbox("Enabled##npc_cover", &cover->enabled);
        ImGui::DragFloat3("Peek local offset##npc_cover", &cover->peekOffset.x, 0.05f);
        ImGui::Checkbox("Allow opposite peek##npc_cover",
            &cover->allowOppositePeek);
        ImGui::DragFloat("Protection check height##npc_cover", &cover->protectionHeight,
            0.05f, 0.2f, 2.5f, "%.2f m");
        ImGui::Checkbox("Crouched pose##npc_cover", &cover->crouched);
        ImGui::TextDisabled("Place marker behind collider; offset must reach a visible firing angle.");
    }
    if (auto* point = registry.GetComponent<NpcPatrolPointComponent>(entity))
    {
        ImGui::SeparatorText("NPC Patrol Point");
        finishJump(wantsJump("patrol"));
        ImGui::Checkbox("Enabled##npc_patrol_point", &point->enabled);
        ImGui::DragInt("Route ID##npc_patrol_point", &point->routeId, 1.0f, 0, 10000);
        ImGui::DragInt("Order##npc_patrol_point", &point->order, 1.0f, 0, 10000);
        ImGui::DragFloat("Wait time##npc_patrol_point",
            &point->waitTime, 0.1f, 0.0f, 60.0f, "%.1f s");
        ImGui::TextDisabled(
            "NPCs patrol points with the same Route ID in ascending Order.");
    }

    // Animation setup belongs to every animated object, not just NPC AI. This
    // makes it possible to configure a character before its AI components are
    // attached. SceneManager already persists these bindings in scene JSON.
    if (auto* animation = registry.GetComponent<AnimationComponent>(entity))
    {
        const bool jumpHere = prepareJump("animation");
        const bool animationOpen = ImGui::CollapsingHeader(
            "Animation", ImGuiTreeNodeFlags_DefaultOpen);
        finishJump(jumpHere);
        if (animationOpen)
        {
            auto* mesh = registry.GetComponent<MeshRendererComponent>(entity);
            AssimpModelClass* model = mesh ? mesh->model : nullptr;
            WeaponComponent* presetWeapon = registry.GetComponent<WeaponComponent>(entity);
            if (!presetWeapon)
                if (auto* equipped = registry.GetComponent<EquippedWeaponComponent>(entity))
                    presetWeapon = registry.GetComponent<WeaponComponent>(equipped->weapon);
            if (!presetWeapon)
                registry.ForEach<WeaponOwnerComponent>(
                    [&](Entity candidate, WeaponOwnerComponent& owner)
                    {
                        if (!presetWeapon && owner.owner == entity)
                            presetWeapon = registry.GetComponent<WeaponComponent>(candidate);
                    });
            ImGui::Text("Current: %s", AnimationStateName(animation->currentState));

            ImGui::BeginDisabled(!model || !model->HasAnimations());
            if (ImGui::Button("Auto-detect Idle / Walk / Run", ImVec2(-1.0f, 0.0f)))
            {
                for (AnimationBinding& binding : animation->animations)
                {
                    if (binding.state != AnimationState::Idle &&
                        binding.state != AnimationState::Walk &&
                        binding.state != AnimationState::Run)
                        continue;

                    const int clipIndex = model->FindAnimationIndexBySemanticName(
                        AnimationStateName(binding.state));
                    if (const auto* clip = model->GetAnimation(clipIndex))
                    {
                        binding.name = clip->name;
                        binding.index = clipIndex;
                    }
                }
            }
            ImGui::EndDisabled();

            if (!model || !model->HasAnimations())
                ImGui::TextDisabled("The selected object has no loaded animation clips.");
            else
            {
                ImGui::TextDisabled("Available clips:");
                for (size_t i = 0; i < model->GetAnimationCount(); ++i)
                {
                    if (const auto* clip = model->GetAnimation(static_cast<int>(i)))
                        ImGui::BulletText("%s", clip->name.c_str());
                }
            }

            for (AnimationBinding& binding : animation->animations)
            {
                ImGui::PushID(static_cast<int>(binding.state));
                char clipName[128]{};
                const bool rifleIdle = binding.state == AnimationState::RifleIdle;
                const bool rifleGun = binding.state == AnimationState::RifleGun;
                const std::string& displayedName = binding.name.empty() && presetWeapon
                    ? (rifleIdle ? presetWeapon->rifleIdleAnimation
                        : rifleGun ? presetWeapon->rifleGunAnimation : binding.name)
                    : binding.name;
                strncpy_s(clipName, displayedName.c_str(), _TRUNCATE);
                if (ImGui::InputText(AnimationStateName(binding.state), clipName, sizeof(clipName)))
                {
                    binding.name = clipName;
                    binding.index = -1;
                    if (presetWeapon && rifleIdle)
                        presetWeapon->rifleIdleAnimation = binding.name;
                    if (presetWeapon && rifleGun)
                        presetWeapon->rifleGunAnimation = binding.name;
                }
                ImGui::SameLine();
                if (model && model->HasAnimations())
                    binding.index = model->FindAnimationIndexBySemanticName(displayedName);
                if (binding.state == AnimationState::RifleRun &&
                    displayedName.empty())
                    ImGui::TextDisabled("optional: regular Run + rifle stance");
                else if (binding.index >= 0)
                    ImGui::TextColored(COL_GREEN, "found (%d)", binding.index);
                else
                    ImGui::TextColored(COL_RED, "clip not found");
                if (model && model->HasAnimations())
                {
                    if (ImGui::BeginCombo("##clips", "Choose imported clip"))
                    {
                        for (size_t i = 0; i < model->GetAnimationCount(); ++i)
                        {
                            const auto* clip = model->GetAnimation(static_cast<int>(i));
                            if (!clip) continue;
                            if (ImGui::Selectable(clip->name.c_str(),
                                clip->name == displayedName))
                            {
                                binding.name = clip->name;
                                binding.index = static_cast<int>(i);
                                if (presetWeapon && rifleIdle)
                                    presetWeapon->rifleIdleAnimation = binding.name;
                                if (presetWeapon && rifleGun)
                                    presetWeapon->rifleGunAnimation = binding.name;
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::SameLine();
                    ImGui::BeginDisabled(binding.index < 0);
                    if (ImGui::Button("Preview"))
                    {
                        animation->currentState = binding.state;
                        animation->animationIndex = binding.index;
                        animation->upperBodyAnimationIndex = -1;
                        animation->currentTime = 0.0f;
                        animation->isPlaying = true;
                    }
                    ImGui::EndDisabled();
                }
                ImGui::PopID();
            }
            if (model)
            {
                if (const auto* preview = model->GetAnimation(animation->animationIndex))
                {
                    const float rate = preview->ticksPerSecond > 0.0f
                        ? preview->ticksPerSecond : 24.0f;
                    const float duration = preview->duration / rate;
                    if (duration > 0.0f)
                        ImGui::SliderFloat("Preview time (s)",
                            &animation->currentTime, 0.0f, duration);
                }
            }
            ImGui::TextDisabled("Choose an imported clip or type its name, then save the scene.");
            ImGui::TextDisabled("Rifle bindings control the equipped weapon in Play. Preview time scrubs the full clip in Editor.");
        }
    }


    if (auto* faction =
        registry.GetComponent<FactionComponent>(entity))
    {
        ImGui::SeparatorText("Faction");
        finishJump(wantsJump("faction"));

        static const char* factionNames[] =
        {
            "Iridian",
            "Sirius",
            "Trade",
            "Neutral"
        };

        int selected =
            static_cast<int>(faction->faction);

        if (ImGui::Combo(
            "Faction##npc",
            &selected,
            factionNames,
            IM_ARRAYSIZE(factionNames)))
        {
            faction->faction =
                static_cast<FactionId>(selected);
        }

        if (ImGui::Button(
            "Remove Faction Component",
            ImVec2(-1.0f, 0.0f)))
        {
            registry.RemoveComponent<FactionComponent>(entity);
        }
    }

    if (auto* crew = registry.GetComponent<CrewMemberComponent>(entity))
    {
        const bool jumpHere = prepareJump("crew");
        const bool crewOpen = ImGui::CollapsingHeader(
            "Crew Member", ImGuiTreeNodeFlags_DefaultOpen);
        finishJump(jumpHere);
        if (crewOpen)
        {
            const char* roles[] = {
                "Generalist", "Engineer", "Operations", "Security", "Medic" };
            int role = static_cast<int>(crew->role);
            if (ImGui::Combo("Ship role", &role, roles, IM_ARRAYSIZE(roles)))
                crew->role = static_cast<CrewRole>(role);
            ImGui::Checkbox("On duty", &crew->onDuty);
            ImGui::SliderFloat(
                "Repair skill", &crew->repairSkill, 0.25f, 2.0f, "%.2f x");
            ImGui::TextDisabled(
                "Assigned task: %s",
                EditorEntityDebugLabel(registry, crew->assignedTask).c_str());
            ImGui::TextDisabled("Completed tasks: %u", crew->completedTasks);
            ImGui::TextWrapped(
                "The crew coordinator distributes incidents between on-duty NPCs. "
                "Engineers receive the strongest repair affinity.");
            if (ImGui::Button("Remove Crew Member", ImVec2(-1.0f, 0.0f)))
                registry.RemoveComponent<CrewMemberComponent>(entity);
        }
    }
    else if (registry.HasComponent<NPCComponent>(entity) &&
        ImGui::Button("Add Crew Member", ImVec2(-1.0f, 0.0f)))
    {
        registry.AddComponent<CrewMemberComponent>(entity, CrewMemberComponent{});
    }

    // One common physics panel appears for every character entity. This keeps
    // Player and NPC tuning in sync and avoids two subtly different inspectors.
    if (registry.HasComponent<CharacterController>(entity))
    {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.24f, 0.16f, 0.05f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.36f, 0.23f, 0.07f, 1.0f));
        const bool jumpHere = prepareJump("character_physics");
        const bool physicsOpen = ImGui::CollapsingHeader(
            "Character Physics (Player + NPC)", ImGuiTreeNodeFlags_DefaultOpen);
        finishJump(jumpHere);
        ImGui::PopStyleColor(2);
        if (physicsOpen)
            RenderCharacterPhysicsTuning(registry, entity);
    }

    if (auto* trigger = registry.GetComponent<TriggerZoneComponent>(entity))
    {
        const bool jumpHere = prepareJump("trigger");
        const bool triggerOpen = ImGui::CollapsingHeader(
            "Trigger Zone", ImGuiTreeNodeFlags_DefaultOpen);
        finishJump(jumpHere);
        if (triggerOpen)
        {
            ImGui::Checkbox("Enabled##trigger", &trigger->enabled);
            InputStringValue("Event ID##trigger", trigger->eventId);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Stable route consumed by quests, scripts, HUD or other gameplay systems.");

            const char* triggerShapes[] = { "Box", "Sphere" };
            int shape = static_cast<int>(trigger->shape);
            if (ImGui::Combo("Shape##trigger", &shape, triggerShapes, IM_ARRAYSIZE(triggerShapes)))
                trigger->shape = static_cast<TriggerZoneShape>(shape);

            ImGui::DragFloat3("Local center##trigger", &trigger->localCenter.x, 0.05f);
            if (trigger->shape == TriggerZoneShape::Box)
            {
                ImGui::DragFloat3(
                    "Half extents##trigger", &trigger->boxHalfExtents.x,
                    0.05f, 0.05f, 500.0f);
                trigger->boxHalfExtents.x = (std::max)(0.05f, trigger->boxHalfExtents.x);
                trigger->boxHalfExtents.y = (std::max)(0.05f, trigger->boxHalfExtents.y);
                trigger->boxHalfExtents.z = (std::max)(0.05f, trigger->boxHalfExtents.z);
                ImGui::TextDisabled(
                    "Full size: %.1f x %.1f x %.1f",
                    trigger->boxHalfExtents.x * 2.0f,
                    trigger->boxHalfExtents.y * 2.0f,
                    trigger->boxHalfExtents.z * 2.0f);
                if (ImGui::SmallButton("3 x 2 x 3##trigger"))
                    trigger->boxHalfExtents = { 1.5f, 1.0f, 1.5f };
                ImGui::SameLine();
                if (ImGui::SmallButton("10 x 3 x 10##trigger"))
                    trigger->boxHalfExtents = { 5.0f, 1.5f, 5.0f };
            }
            else
            {
                ImGui::DragFloat(
                    "Radius##trigger", &trigger->sphereRadius,
                    0.05f, 0.05f, 500.0f, "%.2f m");
                trigger->sphereRadius = (std::max)(0.05f, trigger->sphereRadius);
            }

            ImGui::SeparatorText("Accepted actors");
            std::uint32_t actorMask = static_cast<std::uint32_t>(trigger->actorMask);
            auto actorFlag = [&](const char* label, TriggerZoneActorMask flag)
            {
                const std::uint32_t bit = static_cast<std::uint32_t>(flag);
                bool accepted = (actorMask & bit) != 0;
                if (ImGui::Checkbox(label, &accepted))
                {
                    if (accepted) actorMask |= bit;
                    else actorMask &= ~bit;
                }
            };
            actorFlag("Player##trigger", TriggerZoneActorMask::Player);
            ImGui::SameLine();
            actorFlag("NPC##trigger", TriggerZoneActorMask::Npc);
            ImGui::SameLine();
            actorFlag("Other##trigger", TriggerZoneActorMask::Other);
            trigger->actorMask = static_cast<TriggerZoneActorMask>(actorMask);

            ImGui::SeparatorText("Event behaviour");
            ImGui::Checkbox("One shot##trigger", &trigger->oneShot);
            ImGui::DragFloat(
                "Cooldown##trigger", &trigger->cooldownSeconds,
                0.1f, 0.0f, 3600.0f, "%.1f s");
            trigger->cooldownSeconds = (std::max)(0.0f, trigger->cooldownSeconds);
            ImGui::Checkbox("Emit Stay events##trigger", &trigger->emitStay);
            ImGui::BeginDisabled(!trigger->emitStay);
            ImGui::DragFloat(
                "Stay interval##trigger", &trigger->stayIntervalSeconds,
                0.1f, 0.0f, 60.0f, "%.1f s");
            trigger->stayIntervalSeconds = (std::max)(0.0f, trigger->stayIntervalSeconds);
            ImGui::EndDisabled();

            const std::string linkedLabel = EditorEntityDebugLabel(registry, trigger->linkedEntity);
            if (ImGui::BeginCombo("Linked entity##trigger", linkedLabel.c_str()))
            {
                const bool noneSelected = trigger->linkedEntity == INVALID_ENTITY;
                if (ImGui::Selectable("None", noneSelected))
                    trigger->linkedEntity = INVALID_ENTITY;
                for (auto& [candidate, candidateTransform] : registry.GetComponents<TransformComponent>())
                {
                    if (candidate == entity)
                        continue;
                    const bool selected = trigger->linkedEntity == candidate;
                    const bool importedShipPart =
                        registry.HasComponent<ShipPartComponent>(candidate);
                    std::string candidateLabel =
                        EditorEntityDebugLabel(registry, candidate);
                    if (importedShipPart)
                        candidateLabel += " (imported GLB part - unavailable)";
                    ImGui::BeginDisabled(importedShipPart);
                    if (ImGui::Selectable(candidateLabel.c_str(), selected))
                        trigger->linkedEntity = candidate;
                    ImGui::EndDisabled();
                }
                ImGui::EndCombo();
            }

            if (trigger->linkedEntity != INVALID_ENTITY &&
                registry.IsAlive(trigger->linkedEntity) &&
                registry.HasComponent<ShipPartComponent>(trigger->linkedEntity))
            {
                ImGui::TextColored(
                    COL_RED,
                    "Unsafe reference: imported GLB parts are regenerated and "
                    "their entity IDs are not persisted.");
                ImGui::TextWrapped(
                    "The legacy target remains visible, but choose a stable root "
                    "or regular scene entity before saving.");
            }

            ImGui::SeparatorText("Editor visualization");
            ImGui::Checkbox("Show debug shape##trigger", &trigger->showDebugShape);
            ImGui::ColorEdit4("Debug color##trigger", &trigger->debugColor.x);

            if (ImGui::Button("Remove Trigger Zone", ImVec2(-1.0f, 0.0f)))
                registry.RemoveComponent<TriggerZoneComponent>(entity);
        }
    }

    if (auto* action = registry.GetComponent<TriggerActionComponent>(entity))
    {
        const bool jumpHere = prepareJump("trigger_action");
        const bool actionOpen = ImGui::CollapsingHeader(
            "Trigger Action", ImGuiTreeNodeFlags_DefaultOpen);
        finishJump(jumpHere);
        if (actionOpen)
        {
            const char* actions[] = {
                "Emit event only",
                "Show HUD message",
                "Set entity active",
                "Toggle entity active",
                "Set door open" };
            int actionType = static_cast<int>(action->action);
            if (ImGui::Combo(
                "Action", &actionType, actions, IM_ARRAYSIZE(actions)))
            {
                action->action = static_cast<TriggerActionType>(actionType);
            }

            ImGui::Checkbox("On Enter", &action->executeOnEnter);
            ImGui::SameLine();
            ImGui::Checkbox("On Stay", &action->executeOnStay);
            ImGui::SameLine();
            ImGui::Checkbox("On Exit", &action->executeOnExit);

            if (action->action == TriggerActionType::ShowHudMessage)
            {
                InputStringValue("HUD message", action->hudMessage);
                ImGui::DragFloat(
                    "Duration", &action->messageDuration,
                    0.1f, 0.1f, 30.0f, "%.1f s");
            }
            else if (action->action != TriggerActionType::None)
            {
                const std::string targetLabel =
                    EditorEntityDebugLabel(registry, action->target);
                if (ImGui::BeginCombo("Target entity", targetLabel.c_str()))
                {
                    if (ImGui::Selectable(
                        "Use Trigger linked entity",
                        action->target == INVALID_ENTITY))
                    {
                        action->target = INVALID_ENTITY;
                    }
                    for (auto& [candidate, transform] :
                        registry.GetComponents<TransformComponent>())
                    {
                        (void)transform;
                        if (candidate == entity)
                            continue;
                        const bool importedShipPart =
                            registry.HasComponent<ShipPartComponent>(candidate);
                        std::string label =
                            EditorEntityDebugLabel(registry, candidate);
                        if (importedShipPart)
                            label += " (imported GLB part - unavailable)";
                        ImGui::BeginDisabled(importedShipPart);
                        if (ImGui::Selectable(
                            label.c_str(), action->target == candidate))
                        {
                            action->target = candidate;
                        }
                        ImGui::EndDisabled();
                    }
                    ImGui::EndCombo();
                }

                if (action->target != INVALID_ENTITY &&
                    registry.IsAlive(action->target) &&
                    registry.HasComponent<ShipPartComponent>(action->target))
                {
                    ImGui::TextColored(
                        COL_RED,
                        "Unsafe reference: imported GLB parts are regenerated and "
                        "their entity IDs are not persisted.");
                    ImGui::TextWrapped(
                        "The legacy target remains visible, but select a stable "
                        "root or regular scene entity before saving.");
                }

                if (action->action != TriggerActionType::ToggleEntityActive)
                    ImGui::Checkbox("Target value", &action->value);
                ImGui::Checkbox("Invert value on Exit", &action->invertOnExit);
            }

            ImGui::TextWrapped(
                "Built-in actions run without gameplay code. Event ID is still "
                "emitted for future quest or scripting systems.");
            if (ImGui::Button("Remove Trigger Action", ImVec2(-1.0f, 0.0f)))
                registry.RemoveComponent<TriggerActionComponent>(entity);
        }
    }
    else if (registry.HasComponent<TriggerZoneComponent>(entity) &&
        ImGui::Button("Add Trigger Action", ImVec2(-1.0f, 0.0f)))
    {
        registry.AddComponent<TriggerActionComponent>(
            entity, TriggerActionComponent{});
    }

    if (auto* room = registry.GetComponent<RoomComponent>(entity))
    {
        const bool jumpHere = prepareJump("room");
        const bool roomOpen = ImGui::CollapsingHeader(
            "Room Volume", ImGuiTreeNodeFlags_DefaultOpen);
        finishJump(jumpHere);
        if (roomOpen)
        {
            char roomName[160]{};
            strncpy_s(roomName, sizeof(roomName), room->roomName.c_str(), _TRUNCATE);
            if (ImGui::InputText("Room name", roomName, sizeof(roomName)))
            {
                room->roomName = roomName;
                if (auto* name = registry.GetComponent<NameComponent>(entity))
                    name->name = roomName;
            }

            ImGui::SeparatorText("Local bounds");
            ImGui::DragFloat3("Minimum", &room->localMin.x, 0.05f);
            ImGui::DragFloat3("Maximum", &room->localMax.x, 0.05f);
            room->localMax.x = (std::max)(room->localMax.x, room->localMin.x + 0.05f);
            room->localMax.y = (std::max)(room->localMax.y, room->localMin.y + 0.05f);
            room->localMax.z = (std::max)(room->localMax.z, room->localMin.z + 0.05f);

            if (ImGui::SmallButton("2 x 2 x 2"))
            {
                room->localMin = { -1.0f, -1.0f, -1.0f };
                room->localMax = { 1.0f, 1.0f, 1.0f };
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("5 x 3 x 5"))
            {
                room->localMin = { -2.5f, -1.5f, -2.5f };
                room->localMax = { 2.5f, 1.5f, 2.5f };
            }

            ImGui::SeparatorText("Environment defaults");
            float oxygenPercent = room->oxygen * 100.0f;
            if (ImGui::SliderFloat(
                "Oxygen", &oxygenPercent, 0.0f, 100.0f,
                "%.0f%%", ImGuiSliderFlags_AlwaysClamp))
            {
                room->oxygen = oxygenPercent * 0.01f;
            }
            float pressurePercent = room->pressure * 100.0f;
            if (ImGui::SliderFloat(
                "Pressure", &pressurePercent, 0.0f, 100.0f,
                "%.0f%%", ImGuiSliderFlags_AlwaysClamp))
            {
                room->pressure = pressurePercent * 0.01f;
            }
            ImGui::DragFloat("Temperature", &room->temperature, 0.1f, -100.0f, 200.0f, "%.1f C");
            ImGui::DragFloat("Leak rate", &room->leakRate, 0.01f, 0.0f, 100.0f);
            ImGui::Checkbox("Simulation active", &room->isActive);
            ImGui::TextDisabled("Connected rooms: %d", static_cast<int>(room->connectedRooms.size()));

            if (ImGui::Button("Remove Room Component", ImVec2(-1.0f, 0.0f)))
                registry.RemoveComponent<RoomComponent>(entity);
        }
    }


    if (auto* surface = registry.GetComponent<NavMeshSurfaceComponent>(entity))
    {
        const bool jumpHere = prepareJump("navmesh");
        const bool surfaceOpen = ImGui::CollapsingHeader(
            "NavMesh Surface", ImGuiTreeNodeFlags_DefaultOpen);
        finishJump(jumpHere);
        if (surfaceOpen)
        {
            ImGui::Checkbox("Walkable collision surface", &surface->enabled);
            ImGui::TextWrapped("Requires a static Rigid Body or Static Collider. NPCs can navigate here without a Room Component.");
            if (ImGui::Button("Remove NavMesh Surface", ImVec2(-1.0f, 0.0f)))
                registry.RemoveComponent<NavMeshSurfaceComponent>(entity);
        }
    }

    if (registry.HasComponent<WorldAreaComponent>(entity))
    {
        auto* area = registry.GetComponent<WorldAreaComponent>(entity);

        if (area)
        {
            const char* areaTypes[] = { "Grass", "No Grass", "Forest", "Spawn", "Trigger" };
            int type = static_cast<int>(area->type);

            ImGui::SeparatorText("World Area");
            finishJump(wantsJump("world_area"));
            if (ImGui::Combo("Type", &type, areaTypes, IM_ARRAYSIZE(areaTypes)))
                area->type = static_cast<WorldAreaType>(type);

            const char* areaHelp = "Defines a generic world volume.";
            if (area->type == WorldAreaType::Trigger)
                areaHelp = "Trigger volume: use this region for enter/exit gameplay events.";
            else if (area->type == WorldAreaType::Spawn)
                areaHelp = "Spawn volume: marks a valid region for character or object spawning.";
            else if (area->type == WorldAreaType::Grass || area->type == WorldAreaType::Forest)
                areaHelp = "Procedural vegetation volume. Density and seed control placement.";
            else if (area->type == WorldAreaType::NoGrass)
                areaHelp = "Exclusion volume that removes vegetation from this region.";
            ImGui::TextWrapped("%s", areaHelp);

            if (ImGui::DragFloat3("Center", &area->center.x, 0.1f) && m_transformSnapEnabled)
            {
                area->center.x = SnapEditorValue(area->center.x, m_positionSnap);
                area->center.y = SnapEditorValue(area->center.y, m_positionSnap);
                area->center.z = SnapEditorValue(area->center.z, m_positionSnap);
            }
            ImGui::DragFloat3("Size", &area->size.x, 0.1f, 0.1f, 500.0f);
            if (ImGui::SmallButton("3 x 2 x 3"))
                area->size = { 3.0f, 2.0f, 3.0f };
            ImGui::SameLine();
            if (ImGui::SmallButton("10 x 2 x 10"))
                area->size = { 10.0f, 2.0f, 10.0f };

            if (area->type == WorldAreaType::Grass || area->type == WorldAreaType::Forest)
            {
                ImGui::DragFloat("Density", &area->density, 0.01f, 0.0f, 20.0f);
                ImGui::InputScalar("Seed", ImGuiDataType_U32, &area->seed);
            }
            ImGui::Checkbox("Enabled", &area->enabled);
        }
    }

    // -- HullPanelComponent ----------------------------------------------------
    if (auto* panel = registry.GetComponent<HullPanelComponent>(entity))
    {
        const bool jumpHere = prepareJump("hull_panel");
        const bool panelOpen = ImGui::CollapsingHeader(
            "Hull Panel Component", ImGuiTreeNodeFlags_DefaultOpen);
        finishJump(jumpHere);
        if (panelOpen)
        {
            char panelName[160]{};
            strncpy_s(panelName, panel->panelName.c_str(), _TRUNCATE);
            if (ImGui::InputText("Panel name", panelName, sizeof(panelName)))
                panel->panelName = panelName;

            ImGui::DragFloat("Max health", &panel->maxHealth, 1.0f, 1.0f, 100000.0f, "%.1f");
            panel->health = (std::min)(panel->health, panel->maxHealth);
            ImGui::DragFloat("Current health", &panel->health, 1.0f, 0.0f, panel->maxHealth, "%.1f");
            ImGui::DragFloat("Armor resistance", &panel->armorResistance, 0.1f, 0.01f, 100.0f, "%.2fx");
            ImGui::DragInt("Maximum breaches", &panel->maxBreaches, 1.0f, 1, 32);
            ImGui::Checkbox("External panel", &panel->isExternal);

            const float damagePercent = (1.0f - panel->Health01()) * 100.0f;
            ImGui::ProgressBar(panel->Health01(), ImVec2(-1.0f, 0.0f));
            ImGui::Text("Structural damage: %.0f%%", damagePercent);
            ImGui::Text("Breaches: %d / %d", panel->breachCount, panel->maxBreaches);

            if (ImGui::Button("Reset Hull Damage", ImVec2(-1.0f, 0.0f)))
            {
                panel->health = panel->maxHealth;
                panel->accumulatedDamage = 0.0f;
                panel->breachCount = 0;
                panel->isDestroyed = false;
                if (auto* decals = registry.GetComponent<DamageDecalComponent>(entity))
                    decals->cracks.clear();
            }
        }
    }
    // -- Weapon / inventory authoring ------------------------------------------
    if (auto* weapon = registry.GetComponent<WeaponComponent>(entity))
    {
        const bool jumpHere = prepareJump("weapon");
        const bool weaponOpen = ImGui::CollapsingHeader(
            "Weapon", ImGuiTreeNodeFlags_DefaultOpen);
        finishJump(jumpHere);
        if (weaponOpen)
        {
            const char* weaponTypes[] = { "Handheld", "Ship Cannon" };
            int selectedType = static_cast<int>(weapon->type);
            if (ImGui::Combo(
                "Weapon type##standalone_weapon",
                &selectedType,
                weaponTypes,
                IM_ARRAYSIZE(weaponTypes)))
            {
                weapon->type = static_cast<WeaponType>(selectedType);
            }

            ImGui::DragFloat(
                "Damage##standalone_weapon",
                &weapon->damage,
                0.5f,
                0.01f,
                2000.0f);
            ImGui::DragFloat(
                "Range##standalone_weapon",
                &weapon->range,
                1.0f,
                0.1f,
                5000.0f,
                "%.1f m");
            ImGui::DragFloat(
                "Fire rate##standalone_weapon",
                &weapon->fireRate,
                0.01f,
                0.01f,
                20.0f);
            if (weapon->type == WeaponType::ShipCannon)
            {
                ImGui::DragFloat(
                    "Breach radius##standalone_weapon",
                    &weapon->breachRadius,
                    0.01f,
                    0.1f,
                    5.0f);
            }

            if (!registry.HasComponent<PlayerGameTag>(entity))
            {
                const auto* owner = registry.GetComponent<WeaponOwnerComponent>(entity);
                auto* ownerAnimation = owner
                    ? registry.GetComponent<AnimationComponent>(owner->owner) : nullptr;
                const auto* ownerMesh = owner
                    ? registry.GetComponent<MeshRendererComponent>(owner->owner) : nullptr;
                DrawWeaponProfileControls(sceneManager, *weapon, ownerAnimation,
                    m_weaponProfileStatus, m_weaponVisualReloadRequested);
                DrawWeaponGripEditor(*weapon, ownerAnimation,
                    ownerMesh ? ownerMesh->model : nullptr,
                    &m_weaponVisualReloadRequested);
            }

            if (ImGui::Button("Remove Weapon", ImVec2(-1.0f, 0.0f)))
                registry.RemoveComponent<WeaponComponent>(entity);
        }
    }

    if (auto* pickup = registry.GetComponent<WeaponPickupComponent>(entity))
    {
        const bool jumpHere = prepareJump("pickup");
        const bool pickupOpen = ImGui::CollapsingHeader(
            "Weapon Pickup", ImGuiTreeNodeFlags_DefaultOpen);
        finishJump(jumpHere);
        if (pickupOpen)
        {
            Entity storedOwner = INVALID_ENTITY;
            if (const auto* ownership =
                registry.GetComponent<WeaponOwnerComponent>(entity))
            {
                if (ownership->owner != INVALID_ENTITY &&
                    registry.IsAlive(ownership->owner))
                {
                    const auto* ownerInventory =
                        registry.GetComponent<InventoryComponent>(ownership->owner);
                    if (ownerInventory && ownerInventory->Contains(entity))
                        storedOwner = ownership->owner;
                }
            }

            const bool isStored = storedOwner != INVALID_ENTITY;
            ImGui::BeginDisabled(isStored);
            ImGui::Checkbox("Enabled##weapon_pickup", &pickup->enabled);
            ImGui::EndDisabled();
            ImGui::Checkbox("Equip on pickup", &pickup->equipOnPickup);
            ImGui::Checkbox("Create physics on drop", &pickup->createPhysicsOnDrop);
            ImGui::BeginDisabled(!pickup->createPhysicsOnDrop);
            ImGui::DragFloat(
                "Drop mass",
                &pickup->dropMass,
                0.05f,
                0.01f,
                100.0f,
                "%.2f kg");
            ImGui::DragFloat3(
                "Drop collision size",
                &pickup->dropCollisionSize.x,
                0.01f,
                0.05f,
                10.0f,
                "%.2f m");
            ImGui::EndDisabled();

            char promptBuffer[256]{};
            strncpy_s(promptBuffer, pickup->prompt.c_str(), _TRUNCATE);
            if (ImGui::InputText(
                "Pickup prompt",
                promptBuffer,
                sizeof(promptBuffer)))
            {
                pickup->prompt = promptBuffer;
                if (auto* interaction =
                    registry.GetComponent<InteractionComponent>(entity))
                {
                    interaction->prompt = pickup->prompt;
                }
            }

            if (isStored)
            {
                const bool hasWorldState =
                    pickup->enabled ||
                    registry.HasComponent<ActiveComponent>(entity) ||
                    registry.HasComponent<InteractionComponent>(entity) ||
                    registry.HasComponent<RigidBodyComponent>(entity);
                ImGui::TextColored(
                    hasWorldState ? COL_RED : COL_GREEN,
                    "Stored by %s",
                    EditorEntityDebugLabel(registry, storedOwner).c_str());
                if (hasWorldState)
                {
                    ImGui::TextWrapped(
                        "Stored weapons cannot also be active, physical world pickups.");
                    if (ImGui::Button(
                        "Repair Stored State", ImVec2(-1.0f, 0.0f)))
                    {
                        SetEditorStoredWeaponState(registry, entity);
                        SetEditorStatus("Repaired stored weapon world state");
                    }
                }
            }
            else
            {
                const bool validPickup =
                    pickup->enabled &&
                    registry.HasComponent<WeaponComponent>(entity) &&
                    registry.HasComponent<InteractionComponent>(entity);
                ImGui::TextColored(
                    validPickup ? COL_GREEN : COL_RED,
                    validPickup
                        ? "Ready for Play mode"
                        : "World pickup requires Enabled, Weapon and Interaction");

                if (ImGui::Button("Repair Pickup Setup", ImVec2(-1.0f, 0.0f)))
                {
                    pickup->enabled = true;
                    if (!registry.HasComponent<WeaponComponent>(entity))
                        registry.AddComponent<WeaponComponent>(entity, WeaponComponent{});
                    if (!registry.HasComponent<InteractionComponent>(entity))
                    {
                        InteractionComponent interaction;
                        interaction.prompt = pickup->prompt;
                        interaction.maxDistance = 3.0f;
                        registry.AddComponent<InteractionComponent>(
                            entity, std::move(interaction));
                    }
                    registry.AddComponent<ActiveComponent>(entity, ActiveComponent{});
                }
            }

            if (ImGui::Button("Remove Weapon Pickup", ImVec2(-1.0f, 0.0f)))
                registry.RemoveComponent<WeaponPickupComponent>(entity);
        }
    }

    if (auto* inventory = registry.GetComponent<InventoryComponent>(entity))
    {
        const bool jumpHere = prepareJump("inventory");
        const bool inventoryOpen = ImGui::CollapsingHeader(
            "Inventory (3 Slots)", ImGuiTreeNodeFlags_DefaultOpen);
        finishJump(jumpHere);
        if (inventoryOpen)
        {
            ImGui::TextDisabled(
                "Choose an existing weapon entity. Stored weapons are hidden; "
                "Drop restores them as world pickups.");

            std::vector<Entity> weaponCandidates;
            registry.ForEach<WeaponComponent>(
                [&](Entity candidate, WeaponComponent&)
                {
                    weaponCandidates.push_back(candidate);
                });
            std::sort(weaponCandidates.begin(), weaponCandidates.end());

            for (std::size_t slot = 0;
                slot < InventoryComponent::SlotCount;
                ++slot)
            {
                ImGui::PushID(static_cast<int>(slot));
                Entity item = inventory->slots[slot].item;
                const std::string label = "Slot " + std::to_string(slot + 1);

                ImGui::Text("%s", label.c_str());
                if (inventory->activeSlot == slot)
                {
                    ImGui::SameLine();
                    ImGui::TextColored(COL_ACCENT, "ACTIVE");
                }

                const std::string preview = item != INVALID_ENTITY
                    ? EditorEntityDebugLabel(registry, item)
                    : "<Empty - select a weapon>";
                if (ImGui::BeginCombo("Stored weapon", preview.c_str()))
                {
                    for (Entity candidate : weaponCandidates)
                    {
                        const bool selfReference = candidate == entity;
                        const bool importedShipPart =
                            registry.HasComponent<ShipPartComponent>(candidate);
                        Entity otherOwner = INVALID_ENTITY;
                        if (const auto* ownership =
                            registry.GetComponent<WeaponOwnerComponent>(candidate))
                        {
                            otherOwner = ownership->owner;
                        }
                        const bool ownedElsewhere =
                            otherOwner != INVALID_ENTITY && otherOwner != entity;
                        const bool unavailable =
                            selfReference || importedShipPart || ownedElsewhere;

                        std::string candidateLabel =
                            EditorEntityDebugLabel(registry, candidate);
                        const std::size_t storedSlot =
                            inventory->FindItem(candidate);
                        if (storedSlot != InventoryComponent::InvalidSlot &&
                            storedSlot != slot)
                        {
                            candidateLabel += " (move from slot " +
                                std::to_string(storedSlot + 1) + ")";
                        }
                        if (ownedElsewhere)
                        {
                            candidateLabel += " (owned by " +
                                EditorEntityDebugLabel(registry, otherOwner) + ")";
                        }
                        if (selfReference)
                            candidateLabel += " (inventory owner - unavailable)";
                        if (importedShipPart)
                            candidateLabel += " (imported GLB part - unavailable)";

                        ImGui::BeginDisabled(unavailable);
                        if (ImGui::Selectable(
                            candidateLabel.c_str(), candidate == item))
                        {
                            const InventoryOperation result =
                                StoreEditorWeaponInSlot(
                                    registry, entity, candidate, slot);
                            SetEditorStatus(EditorInventoryStatusMessage(result.status));
                            item = inventory->slots[slot].item;
                        }
                        ImGui::EndDisabled();
                    }
                    if (weaponCandidates.empty())
                        ImGui::TextDisabled("No WeaponComponent entities in this scene.");
                    ImGui::EndCombo();
                }

                const bool validStoredItem =
                    item != INVALID_ENTITY &&
                    registry.IsAlive(item) &&
                    registry.HasComponent<WeaponComponent>(item);
                if (item != INVALID_ENTITY && !validStoredItem)
                {
                    ImGui::TextColored(
                        COL_RED,
                        "Broken reference: use Repair Stored States.");
                }
                else if (validStoredItem)
                {
                    const auto* ownership =
                        registry.GetComponent<WeaponOwnerComponent>(item);
                    const bool wrongOwner =
                        !ownership || ownership->owner != entity;
                    const bool hasWorldState =
                        registry.HasComponent<ActiveComponent>(item) ||
                        registry.HasComponent<InteractionComponent>(item) ||
                        registry.HasComponent<RigidBodyComponent>(item) ||
                        (registry.GetComponent<WeaponPickupComponent>(item) &&
                            registry.GetComponent<WeaponPickupComponent>(item)->enabled);
                    if (wrongOwner || hasWorldState)
                    {
                        ImGui::TextColored(
                            COL_RED,
                            "Stored-state mismatch; use Repair Stored States.");
                    }
                }

                if (inventory->activeSlot != slot)
                {
                    if (ImGui::SmallButton("Make Active"))
                    {
                        const InventoryOperation result =
                            InventorySystem::SelectSlot(registry, entity, slot);
                        if (result || result.status == InventoryStatus::EmptySlot)
                            SetEditorStatus("Selected inventory slot " +
                                std::to_string(slot + 1));
                        else
                            SetEditorStatus(EditorInventoryStatusMessage(result.status));
                    }
                }

                ImGui::SameLine();
                ImGui::BeginDisabled(item == INVALID_ENTITY);
                if (ImGui::SmallButton("Drop To World"))
                {
                    const InventoryOperation result =
                        DropEditorInventorySlot(registry, entity, slot);
                    SetEditorStatus(result
                        ? "Dropped weapon from slot " + std::to_string(slot + 1)
                        : EditorInventoryStatusMessage(result.status));
                    item = inventory->slots[slot].item;
                }
                ImGui::EndDisabled();

                ImGui::Separator();
                ImGui::PopID();
            }

            ImGui::Text("Stored: %zu / %zu",
                inventory->GetItemCount(),
                InventoryComponent::SlotCount);
            if (ImGui::Button("Repair Stored States", ImVec2(-1.0f, 0.0f)))
            {
                InventorySystem::Sanitize(registry, entity);
                for (const InventorySlot& slot : inventory->slots)
                {
                    if (!slot.IsEmpty())
                        SetEditorStoredWeaponState(registry, slot.item);
                }
                SetEditorStatus("Inventory links and stored weapon states repaired");
            }
            if (ImGui::Button("Remove Inventory", ImVec2(-1.0f, 0.0f)))
            {
                for (std::size_t slot = 0;
                    slot < InventoryComponent::SlotCount;
                    ++slot)
                {
                    DropEditorInventorySlot(registry, entity, slot);
                }
                registry.RemoveComponent<InventoryComponent>(entity);
                SetEditorStatus("Inventory removed; stored weapons restored to world");
            }
        }
    }

    // -- InteractionComponent --------------------------------------------------
    if (auto* interaction = registry.GetComponent<InteractionComponent>(entity))
    {
        const bool jumpHere = prepareJump("interaction");
        const bool interactionOpen = ImGui::CollapsingHeader(
            "Interaction", ImGuiTreeNodeFlags_DefaultOpen);
        finishJump(jumpHere);
        if (interactionOpen)
        {
            ImGui::Checkbox("Enabled##interaction", &interaction->enabled);
            ImGui::DragFloat("Max distance##interaction", &interaction->maxDistance,
                0.1f, 0.1f, 100.0f, "%.1f m");

            char promptBuffer[256]{};
            strncpy_s(promptBuffer, interaction->prompt.c_str(), _TRUNCATE);
            if (ImGui::InputText("Prompt##interaction", promptBuffer, sizeof(promptBuffer)))
                interaction->prompt = promptBuffer;

            if (ImGui::Button("Remove Interaction", ImVec2(-1, 0)))
                registry.RemoveComponent<InteractionComponent>(entity);
        }
    }
    else if (ImGui::Button("Add Interaction Component", ImVec2(-1, 0)))
    {
        registry.AddComponent<InteractionComponent>(entity, InteractionComponent{});
    }
    // -- DoorComponent ---------------------------------------------------------
    if (registry.HasComponent<DoorComponent>(entity))
    {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.15f, 0.05f, 1.0f)); // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.35f, 0.22f, 0.08f, 1.0f));
        const bool jumpHere = prepareJump("door");
        bool open = ImGui::CollapsingHeader("  Door Component##door", ImGuiTreeNodeFlags_DefaultOpen);
        finishJump(jumpHere);
        ImGui::PopStyleColor(2);

        if (open)
        {
            auto* door = registry.GetComponent<DoorComponent>(entity);

            ImGui::TextColored(COL_TEXT_DIM, " Resolved room link:");
            std::string resolvedRoomA = EditorEntityDebugLabel(registry, door->roomA);
            std::string resolvedRoomB = EditorEntityDebugLabel(registry, door->roomB);
            ImGui::Text("Room A: %s", resolvedRoomA.c_str());
            ImGui::Text("Room B: %s", resolvedRoomB.c_str());
            std::string leftLeaf = EditorEntityDebugLabel(registry, door->leftLeaf);
            std::string rightLeaf = EditorEntityDebugLabel(registry, door->rightLeaf);
            ImGui::Text("Left leaf: %s", leftLeaf.c_str());
            ImGui::Text("Right leaf: %s", rightLeaf.c_str());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Spacing();

            // -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ A --------------------------------------------------------
            ImGui::TextColored(COL_TEXT_DIM, " Compartment A:");
            ImGui::SameLine();

            std::string nameA = (door->compartmentA != INVALID_ENTITY)
                ? ("Entity " + std::to_string(door->compartmentA)) : "None";

            if (ImGui::Button((nameA + "##DoorA").c_str()))
                ImGui::OpenPopup("##DoorPickA");

            if (ImGui::BeginPopup("##DoorPickA"))
            {
                ImGui::TextColored(COL_TEXT_DIM, "Select Compartment A");
                ImGui::Separator();
                auto& compPool = registry.GetComponents<CompartmentComponent>();
                for (auto& [compEnt, comp] : compPool)
                {
                    if (compEnt == door->compartmentB) continue; // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ B
                    std::string label = "Compartment Entity " + std::to_string(compEnt);
                    if (ImGui::Selectable(label.c_str()))
                        door->compartmentA = compEnt;
                }
                ImGui::EndPopup();
            }

            // -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ B --------------------------------------------------------
            ImGui::TextColored(COL_TEXT_DIM, " Compartment B:");
            ImGui::SameLine();

            std::string nameB = (door->compartmentB != INVALID_ENTITY)
                ? ("Entity " + std::to_string(door->compartmentB)) : "None";

            if (ImGui::Button((nameB + "##DoorB").c_str()))
                ImGui::OpenPopup("##DoorPickB");

            if (ImGui::BeginPopup("##DoorPickB"))
            {
                ImGui::TextColored(COL_TEXT_DIM, "Select Compartment B");
                ImGui::Separator();
                auto& compPool = registry.GetComponents<CompartmentComponent>();
                for (auto& [compEnt, comp] : compPool)
                {
                    if (compEnt == door->compartmentA) continue; // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ A
                    std::string label = "Compartment Entity " + std::to_string(compEnt);
                    if (ImGui::Selectable(label.c_str()))
                        door->compartmentB = compEnt;
                }
                ImGui::EndPopup();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ ------------------------------------------------------
            ImGui::Checkbox("Is Open", &door->isOpen);
            ImGui::Checkbox("Is Broken", &door->isBroken);
            ImGui::Checkbox("Requires Power", &door->requiresPower);

            ImGui::Spacing();

            ImGui::DragFloat("Flow Rate (m?/s)", &door->flowRate, 0.1f, 0.0f, 20.0f);
            ImGui::DragFloat("Open Speed", &door->openSpeed, 0.1f, 0.1f, 10.0f);
            ImGui::DragFloat("Slide Distance", &door->slideDistance, 0.05f, 0.0f, 10.0f);

            // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦)
            ImGui::ProgressBar(door->openProgress, ImVec2(-1, 0), "Open Progress");

            ImGui::Spacing();

            // -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ -----------------------------------------------
            if (ImGui::Button("Open Door", ImVec2(-1, 0))) door->isOpen = true;
            if (ImGui::Button("Close Door", ImVec2(-1, 0))) door->isOpen = false;

            ImGui::Spacing();

            // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
            bool valid = (door->compartmentA != INVALID_ENTITY && door->compartmentB != INVALID_ENTITY);
            if (valid)
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "? Both compartments linked");
            else
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.2f, 1.0f), "? Assign both compartments!");

            ImGui::Spacing();

            // -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ -------------------------------------------------------
            ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button("Remove Door Component", ImVec2(-1, 0)))
                registry.RemoveComponent<DoorComponent>(entity);
            ImGui::PopStyleColor(2);
        }
    }

    
        

    // -- Player ---------------------------------------------------------------
    {
        auto createEquippedWeapon = [&](Entity owner, WeaponComponent weaponTemplate = WeaponComponent{})
        {
            Entity weaponEntity = registry.CreateEntity();
            registry.AddComponent<WeaponComponent>(weaponEntity, weaponTemplate);
            registry.AddComponent<WeaponOwnerComponent>(weaponEntity, WeaponOwnerComponent{ owner });
            registry.AddComponent<ActiveComponent>(weaponEntity, ActiveComponent{});

            if (registry.HasComponent<EquippedWeaponComponent>(owner))
                registry.RemoveComponent<EquippedWeaponComponent>(owner);
            registry.AddComponent<EquippedWeaponComponent>(owner, EquippedWeaponComponent{ weaponEntity });

            return weaponEntity;
        };

        auto ensurePlayerSetup = [&]()
        {
            auto& tags = registry.GetComponents<PlayerGameTag>();
            std::vector<Entity> oldPlayers;
            for (auto& [candidate, tag] : tags)
            {
                if (candidate != entity)
                    oldPlayers.push_back(candidate);
            }
            for (Entity oldPlayer : oldPlayers)
                registry.RemoveComponent<PlayerGameTag>(oldPlayer);

            if (!registry.HasComponent<PlayerGameTag>(entity))
                registry.AddComponent<PlayerGameTag>(entity, PlayerGameTag{});
            if (!registry.HasComponent<PlayerComponent>(entity))
                registry.AddComponent<PlayerComponent>(entity, PlayerComponent{});
            if (!registry.HasComponent<CharacterVitalsComponent>(entity))
                registry.AddComponent<CharacterVitalsComponent>(
                    entity, CharacterVitalsComponent{});
            if (!registry.HasComponent<CharacterMovement>(entity))
                registry.AddComponent<CharacterMovement>(entity, CharacterMovement{});
            if (!registry.HasComponent<CharacterController>(entity))
                registry.AddComponent<CharacterController>(entity, CharacterController{});
            if (!registry.HasComponent<CharacterPhysicsTuningComponent>(entity))
                registry.AddComponent<CharacterPhysicsTuningComponent>(
                    entity, CharacterPhysicsTuningComponent{});
            if (!registry.HasComponent<AnimationComponent>(entity))
                registry.AddComponent<AnimationComponent>(entity, AnimationComponent{});
            if (!registry.HasComponent<PlayCameraComponent>(entity))
                registry.AddComponent<PlayCameraComponent>(
                    entity, PlayCameraComponent{});

            if (!registry.HasComponent<EquippedWeaponComponent>(entity))
            {
                WeaponComponent weaponTemplate{};
                if (auto* legacyWeapon = registry.GetComponent<WeaponComponent>(entity))
                    weaponTemplate = *legacyWeapon;
                createEquippedWeapon(entity, weaponTemplate);
            }

            InventorySystem::EnsureInventory(registry, entity);

            if (registry.HasComponent<WeaponComponent>(entity))
                registry.RemoveComponent<WeaponComponent>(entity);
        };

        const bool isPlayer = registry.HasComponent<PlayerGameTag>(entity);
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.06f, 0.20f, 0.32f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.08f, 0.30f, 0.45f, 1.0f));
        const bool jumpHere = prepareJump("player");
        bool playerOpen = ImGui::CollapsingHeader("  Player##player", isPlayer ? ImGuiTreeNodeFlags_DefaultOpen : 0);
        finishJump(jumpHere);
        ImGui::PopStyleColor(2);

        if (playerOpen)
        {
            if (!isPlayer)
            {
                if (ImGui::Button("Set As Player", ImVec2(-1, 0)))
                    ensurePlayerSetup();
            }
            else
            {
                ImGui::TextColored(COL_ACCENT, "Active player entity");
                if (ImGui::Button("Repair Player Setup", ImVec2(-1, 0)))
                    ensurePlayerSetup();

                if (auto* player = registry.GetComponent<PlayerComponent>(entity))
                {
                    ImGui::SeparatorText("Punch settings");
                    DrawPunchSettings(*player);
                }
                else
                {
                    ImGui::TextColored(COL_RED, "Missing PlayerComponent");
                }

                if (auto* movement = registry.GetComponent<CharacterMovement>(entity))
                {
                    ImGui::Spacing();
                    ImGui::DragFloat("Move Speed", &movement->moveSpeed, 0.1f, 0.0f, 20.0f);
                    ImGui::DragFloat("Run Speed", &movement->runSpeed, 0.1f, 0.0f, 30.0f);
                    ImGui::DragFloat("Rotation Speed", &movement->rotationSpeed, 0.1f, 0.0f, 20.0f);
                    ImGui::SeparatorText("Runtime State");
                    ImGui::Text("Locomotion: %s", PlayerLocomotionStateName(movement->locomotionState));
                    ImGui::Text("Moving: %s", movement->isMoving ? "true" : "false");
                    ImGui::Text("Wants run: %s", movement->wantsRun ? "true" : "false");
                    ImGui::Text("Running: %s", movement->isRunning ? "true" : "false");
                    ImGui::Text("Current speed: %.2f", movement->currentSpeed);
                    if (auto* vitals = registry.GetComponent<CharacterVitalsComponent>(entity))
                        ImGui::Text("Stamina: %.1f / %.1f", vitals->stamina, vitals->maxStamina);
                }
                else
                {
                    ImGui::TextColored(COL_RED, "Missing CharacterMovement");
                }

                ImGui::SeparatorText("Game camera");
                auto* playCamera = registry.GetComponent<PlayCameraComponent>(entity);
                if (!playCamera)
                {
                    if (ImGui::Button("Add Play Camera", ImVec2(-1.0f, 0.0f)))
                    {
                        registry.AddComponent<PlayCameraComponent>(
                            entity, PlayCameraComponent{});
                        playCamera = registry.GetComponent<PlayCameraComponent>(entity);
                    }
                }
                if (playCamera)
                {
                    ImGui::DragFloat("Render Distance (m)##player_camera",
                        &playCamera->renderDistance,
                        1.0f, 1.0f, 10000.0f, "%.0f m");
                    ImGui::DragFloat("Third Person Distance##player_camera",
                        &playCamera->thirdPersonDistance,
                        0.05f, 1.0f, 30.0f, "%.2f m");
                    ImGui::DragFloat3("Third Person Target##player_camera",
                        &playCamera->thirdPersonTargetOffset.x, 0.01f);
                    ImGui::DragFloat3("First Person Eye##player_camera",
                        &playCamera->firstPersonEyeOffset.x, 0.01f);
                    ImGui::TextDisabled(
                        "The same values are available in the Camera sidebar tab.");
                }

                Entity weaponEntity = INVALID_ENTITY;
                if (auto* equipped = registry.GetComponent<EquippedWeaponComponent>(entity))
                {
                    if (ImGui::InputScalar(
                        "Equipped Weapon Entity",
                        ImGuiDataType_U32,
                        &equipped->weapon))
                    {
                        if (equipped->weapon != INVALID_ENTITY &&
                            registry.HasComponent<WeaponComponent>(equipped->weapon))
                        {
                            if (auto* owner = registry.GetComponent<WeaponOwnerComponent>(equipped->weapon))
                                owner->owner = entity;
                            else
                                registry.AddComponent<WeaponOwnerComponent>(
                                    equipped->weapon, WeaponOwnerComponent{ entity });
                        }
                    }
                    weaponEntity = equipped->weapon;
                }

                WeaponComponent* weapon = registry.GetComponent<WeaponComponent>(weaponEntity);
                if (!weapon && registry.HasComponent<WeaponComponent>(entity))
                {
                    WeaponComponent weaponTemplate = *registry.GetComponent<WeaponComponent>(entity);
                    weaponEntity = createEquippedWeapon(entity, weaponTemplate);
                    registry.RemoveComponent<WeaponComponent>(entity);
                    weapon = registry.GetComponent<WeaponComponent>(weaponEntity);
                }

                if (weapon)
                {
                    ImGui::Spacing();
                    const char* weaponTypes[] = { "Handheld", "Ship Cannon" };
                    int selectedType = static_cast<int>(weapon->type);
                    if (ImGui::Combo("Weapon Type", &selectedType, weaponTypes, IM_ARRAYSIZE(weaponTypes)))
                    {
                        weapon->type = static_cast<WeaponType>(selectedType);

                        // Editor presets make the mode switch immediately testable.
                        // The values remain editable after the preset is applied.
                        if (weapon->type == WeaponType::ShipCannon)
                        {
                            weapon->damage = 100.0f;
                            weapon->range = 1000.0f;
                            weapon->fireRate = 0.30f;
                            weapon->breachRadius = 0.65f;
                        }
                        else
                        {
                            weapon->damage = 10.0f;
                            weapon->range = 100.0f;
                            weapon->fireRate = 1.0f;
                        }
                    }

                    ImGui::DragFloat("Damage", &weapon->damage, 0.5f, 0.01f, 2000.0f);
                    ImGui::DragFloat("Range", &weapon->range, 1.0f, 0.1f, 5000.0f);
                    ImGui::DragFloat("Fire Rate", &weapon->fireRate, 0.01f, 0.01f, 20.0f);

                    if (weapon->type == WeaponType::ShipCannon)
                    {
                        ImGui::DragFloat("Breach Radius", &weapon->breachRadius, 0.01f, 0.10f, 5.0f);
                        ImGui::TextColored(COL_GREEN, "Hull penetration enabled");
                    }
                    else
                    {
                        ImGui::TextDisabled("Hull penetration disabled");
                    }

                    auto* ownerAnimation =
                        registry.GetComponent<AnimationComponent>(entity);
                    const auto* ownerMesh =
                        registry.GetComponent<MeshRendererComponent>(entity);
                    DrawWeaponProfileControls(sceneManager, *weapon,
                        ownerAnimation, m_weaponProfileStatus,
                        m_weaponVisualReloadRequested);
                    DrawWeaponGripEditor(*weapon, ownerAnimation,
                        ownerMesh ? ownerMesh->model : nullptr,
                        &m_weaponVisualReloadRequested);
                }
                else
                {
                    ImGui::TextColored(COL_RED, "Missing equipped WeaponComponent");
                    if (ImGui::Button("Create Equipped Weapon", ImVec2(-1, 0)))
                        createEquippedWeapon(entity);
                }

                if (!registry.HasComponent<CharacterController>(entity))
                {
                    ImGui::TextColored(COL_RED, "Missing CharacterController");
                }

                if (!registry.HasComponent<AnimationComponent>(entity))
                    ImGui::TextColored(COL_RED, "Missing AnimationComponent");

                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                if (ImGui::Button("Remove Player Tag", ImVec2(-1, 0)))
                    registry.RemoveComponent<PlayerGameTag>(entity);
                ImGui::PopStyleColor(2);
            }
        }
    }

    // -- First-person view model -----------------------------------------------
    if (auto* firstPerson = registry.GetComponent<FirstPersonViewComponent>(entity))
    {
        const bool jumpHere = prepareJump("first_person");
        const bool firstPersonOpen = ImGui::CollapsingHeader(
            "  First Person View##fp_view",
            ImGuiTreeNodeFlags_DefaultOpen);
        finishJump(jumpHere);
        if (firstPersonOpen)
        {
            Entity playerEntity = INVALID_ENTITY;
            registry.ForEach<PlayerGameTag>(
                [&](Entity candidate, PlayerGameTag&)
                {
                    if (playerEntity == INVALID_ENTITY)
                        playerEntity = candidate;
                });

            ImGui::InputScalar(
                "Owner Entity##fp_view",
                ImGuiDataType_U32,
                &firstPerson->owner);

            if (playerEntity != INVALID_ENTITY && firstPerson->owner != playerEntity)
            {
                ImGui::TextColored(
                    COL_RED,
                    "Owner is not the Player entity (Player: %u)",
                    static_cast<unsigned int>(playerEntity));
                if (ImGui::Button("Use Player As Owner##fp_view", ImVec2(-1.0f, 0.0f)))
                    firstPerson->owner = playerEntity;
            }
            if (registry.HasComponent<PlayerGameTag>(entity))
            {
                ImGui::TextColored(
                    COL_RED,
                    "This is the full Player. Put First Person View on the clipped mesh.");
            }

            ImGui::Checkbox("Follow Camera##fp_view", &firstPerson->followCamera);
            ImGui::Checkbox("Sync Owner Animation##fp_view", &firstPerson->syncAnimation);
            ImGui::Checkbox("Hide Owner In First Person##fp_view", &firstPerson->hideOwner);

            if (firstPerson->followCamera)
            {
                if (ImGui::Button("Reset View Pose##fp_view", ImVec2(-1.0f, 0.0f)))
                {
                    firstPerson->cameraOffset = { 0.0f, -1.0f, -0.22f };
                    firstPerson->rotationOffset = { 0.0f, XM_PI, 0.0f };
                    firstPerson->scale = { 1.0f, 1.0f, 1.0f };
                    firstPerson->hideOwner = true;
                }
                ImGui::DragFloat3(
                    "Camera Offset##fp_view",
                    &firstPerson->cameraOffset.x,
                    0.01f);

                XMFLOAT3 rotationDegrees = {
                    XMConvertToDegrees(firstPerson->rotationOffset.x),
                    XMConvertToDegrees(firstPerson->rotationOffset.y),
                    XMConvertToDegrees(firstPerson->rotationOffset.z)
                };
                if (ImGui::DragFloat3(
                    "Rotation Offset##fp_view",
                    &rotationDegrees.x,
                    0.25f))
                {
                    firstPerson->rotationOffset = {
                        XMConvertToRadians(rotationDegrees.x),
                        XMConvertToRadians(rotationDegrees.y),
                        XMConvertToRadians(rotationDegrees.z)
                    };
                }
                ImGui::DragFloat3(
                    "View Scale##fp_view",
                    &firstPerson->scale.x,
                    0.01f,
                    0.001f,
                    100.0f);
                ImGui::SliderFloat(
                    "Look Up Limit##fp_view", &firstPerson->lookUpLimit,
                    0.0f, 89.0f, "%.1f deg");
                ImGui::SliderFloat(
                    "Look Down Limit##fp_view", &firstPerson->lookDownLimit,
                    0.0f, 89.0f, "%.1f deg");
            }

            ImGui::TextDisabled(
                firstPerson->followCamera
                    ? "Camera anchor (arms/body view model)"
                    : "FP-only attached visual (weapon/accessory)");

            ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
            if (ImGui::Button("Remove First Person View", ImVec2(-1, 0)))
                registry.RemoveComponent<FirstPersonViewComponent>(entity);
            ImGui::PopStyleColor();
        }
    }

    // -- Bone attachment / socket ---------------------------------------------
    if (auto* attachment = registry.GetComponent<BoneAttachmentComponent>(entity))
    {
        const bool jumpHere = prepareJump("bone_attachment");
        const bool attachmentOpen = ImGui::CollapsingHeader(
            "  Bone Attachment##bone_attachment",
            ImGuiTreeNodeFlags_DefaultOpen);
        finishJump(jumpHere);
        if (attachmentOpen)
        {
            ImGui::Checkbox("Enabled##bone_attachment", &attachment->enabled);
            ImGui::InputScalar(
                "Parent Entity##bone_attachment",
                ImGuiDataType_U32,
                &attachment->parent);

            char boneName[128] = {};
            strncpy_s(boneName, attachment->boneName.c_str(), _TRUNCATE);
            if (ImGui::InputText("Bone Name##bone_attachment", boneName, sizeof(boneName)))
                attachment->boneName = boneName;

            ImGui::DragFloat3(
                "Local Position##bone_attachment",
                &attachment->localPosition.x,
                0.005f);

            XMFLOAT3 rotationDegrees = {
                XMConvertToDegrees(attachment->localRotation.x),
                XMConvertToDegrees(attachment->localRotation.y),
                XMConvertToDegrees(attachment->localRotation.z)
            };
            if (ImGui::DragFloat3(
                "Local Rotation##bone_attachment",
                &rotationDegrees.x,
                0.25f))
            {
                attachment->localRotation = {
                    XMConvertToRadians(rotationDegrees.x),
                    XMConvertToRadians(rotationDegrees.y),
                    XMConvertToRadians(rotationDegrees.z)
                };
            }
            ImGui::DragFloat3(
                "Local Scale##bone_attachment",
                &attachment->localScale.x,
                0.01f,
                0.001f,
                100.0f);

            ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
            if (ImGui::Button("Remove Bone Attachment", ImVec2(-1, 0)))
                registry.RemoveComponent<BoneAttachmentComponent>(entity);
            ImGui::PopStyleColor();
        }
    }

    // -- LightComponent --------------------------------------------------------
    if (registry.HasComponent<LightComponent>(entity))
    {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.10f, 0.30f, 0.10f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.14f, 0.40f, 0.14f, 1.0f));
        const bool jumpHere = prepareJump("light");
        bool open = ImGui::CollapsingHeader("  Light Component##lc", ImGuiTreeNodeFlags_DefaultOpen);
        finishJump(jumpHere);
        ImGui::PopStyleColor(2);

        if (open)
        {
            auto* light = registry.GetComponent<LightComponent>(entity);

            ImGui::Spacing();
            float col[4] = { light->color.x, light->color.y, light->color.z, light->color.w };
            ImGui::TextColored(COL_TEXT_DIM, " Color  (w = intensity)");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::ColorEdit4("##LCColor", col))
                light->color = { col[0], col[1], col[2], col[3] };

            ImGui::Spacing();
            ImGui::TextColored(COL_TEXT_DIM, " Radius");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##LCRadius", &light->radius, 0.1f, 0.0f, 500.0f);

            float offset[3] = { light->offset.x, light->offset.y, light->offset.z };
            ImGui::TextColored(COL_TEXT_DIM, " Lens Offset");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat3("##LCOffset", offset, 0.01f))
                light->offset = { offset[0], offset[1], offset[2] };

            bool isSpot = registry.HasComponent<SpotLightComponent>(entity);
            if (ImGui::Checkbox("Spotlight##LC", &isSpot))
            {
                if (isSpot)
                    registry.AddComponent<SpotLightComponent>(entity, SpotLightComponent{});
                else
                    registry.RemoveComponent<SpotLightComponent>(entity);
            }

            if (isSpot)
            {
                auto* spot = registry.GetComponent<SpotLightComponent>(entity);
                if (spot)
                {
                    float direction[3] = {
                        spot->localDirection.x, spot->localDirection.y, spot->localDirection.z };
                    ImGui::TextColored(COL_TEXT_DIM, " Local Direction");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat3("##LCSpotDirection", direction, 0.01f, -1.0f, 1.0f))
                    {
                        XMVECTOR normalized = XMVector3Normalize(XMVectorSet(
                            direction[0], direction[1], direction[2], 0.0f));
                        if (XMVectorGetX(XMVector3LengthSq(normalized)) > 0.000001f)
                            XMStoreFloat3(&spot->localDirection, normalized);
                    }

                    const float presetWidth = ImGui::GetContentRegionAvail().x / 3.0f - 3.0f;
                    if (ImGui::Button("Down -Y##LCSpot", ImVec2(presetWidth, 0.0f)))
                        spot->localDirection = { 0.0f, -1.0f, 0.0f };
                    ImGui::SameLine();
                    if (ImGui::Button("Forward -Z##LCSpot", ImVec2(presetWidth, 0.0f)))
                        spot->localDirection = { 0.0f, 0.0f, -1.0f };
                    ImGui::SameLine();
                    if (ImGui::Button("Flip##LCSpot", ImVec2(-1.0f, 0.0f)))
                    {
                        spot->localDirection.x = -spot->localDirection.x;
                        spot->localDirection.y = -spot->localDirection.y;
                        spot->localDirection.z = -spot->localDirection.z;
                    }

                    ImGui::TextColored(COL_TEXT_DIM, " Inner / Outer Half-Angle");
                    ImGui::SetNextItemWidth(-1);
                    ImGui::DragFloat("##LCSpotInner", &spot->innerAngleDegrees, 0.25f, 0.1f, 89.0f);
                    ImGui::SetNextItemWidth(-1);
                    ImGui::DragFloat("##LCSpotOuter", &spot->outerAngleDegrees, 0.25f, 1.0f, 89.0f);
                    spot->innerAngleDegrees = (std::min)(spot->innerAngleDegrees, spot->outerAngleDegrees);
                }
            }

            ImGui::Spacing();
            ImGui::Checkbox("Enabled##LC", &light->enabled);

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button("Remove##LightComp", ImVec2(-1, 0)))
            {
                registry.RemoveComponent<LightComponent>(entity);
                registry.RemoveComponent<SpotLightComponent>(entity);
            }
            ImGui::PopStyleColor(2);
            ImGui::Spacing();
        }
    }
    if(registry.HasComponent<DamageableComponent>(entity))
    {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.30f, 0.10f, 0.10f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.40f, 0.14f, 0.14f, 1.0f));
        const bool jumpHere = prepareJump("damageable");
        bool open = ImGui::CollapsingHeader("  Damageable Component##dc", ImGuiTreeNodeFlags_DefaultOpen);
        finishJump(jumpHere);
        ImGui::PopStyleColor(2);
        if (open)
        {
            auto* damageable = registry.GetComponent<DamageableComponent>(entity);
            ImGui::Spacing();
            ImGui::TextColored(COL_TEXT_DIM, " Health");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##DCHealth", &damageable->health, 1.0f, 0.0f, 10000.0f);
            ImGui::Spacing();
            ImGui::TextColored(COL_TEXT_DIM, " Max Health");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##DCMaxHealth", &damageable->maxHealth, 1.0f, 1.0f, 10000.0f);
           
            ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button("Remove##DamageableComp", ImVec2(-1, 0)))
                registry.RemoveComponent<DamageableComponent>(entity);
            ImGui::PopStyleColor(2);
            ImGui::Spacing();
        }
	}

    // -- SubmarineComponent --------------------------------------------------------
    if (registry.HasComponent<SubmarineComponent>(entity))
    {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.10f, 0.20f, 0.40f, 1.0f)); // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.14f, 0.28f, 0.55f, 1.0f));
        const bool jumpHere = prepareJump("submarine");
        bool open = ImGui::CollapsingHeader("  Submarine Component##sub", ImGuiTreeNodeFlags_DefaultOpen);
        finishJump(jumpHere);
        ImGui::PopStyleColor(2);

        if (open)
        {
            auto* subComp = registry.GetComponent<SubmarineComponent>(entity);
            if (!subComp) { ImGui::TextColored(COL_RED, "Component error"); return; }

            ImGui::Spacing();

            // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
            if (subComp->submarineRoot == entity)
            {
                ImGui::TextColored(COL_ACCENT, "?? Submarine Root");
                ImGui::Spacing();

                ImGui::TextColored(COL_TEXT_DIM, "Entity ID: %u", entity);
                ImGui::TextColored(COL_GREEN, "This is the submarine root (self-reference)");

                // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                ImGui::TextColored(COL_TEXT_DIM, " Compartments (%zu):", subComp->allCompartments.size());
                for (size_t i = 0; i < subComp->allCompartments.size(); ++i)
                {
                    Entity compEnt = subComp->allCompartments[i];
                    auto* compTransform = registry.GetComponent<TransformComponent>(compEnt);
                    auto* compData = registry.GetComponent<CompartmentComponent>(compEnt);

                    char label[128];
                    if (compData)
                    {
                        float floodPercent = (compData->volume > 0) ? (compData->waterVolume / compData->volume) * 100.f : 0.f;
                        snprintf(label, sizeof(label), "  [%zu] Compartment #%u (%.0f%% flooded)",
                            i, compEnt, floodPercent);
                    }
                    else
                    {
                        snprintf(label, sizeof(label), "  [%zu] Compartment #%u (no data)", i, compEnt);
                    }

                    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦, Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                    bool isSelected = (m_selectedType == SelectedType::Object && m_selectedEntity == compEnt);
                    if (isSelected) ImGui::PushStyleColor(ImGuiCol_Header, COL_SELECTED);

                    if (ImGui::Selectable(label, isSelected))
                    {
                        SetSelection(SelectedType::Object, compEnt);
                    }

                    if (isSelected) ImGui::PopStyleColor();

                    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
                    if (ImGui::SmallButton("Focus##Comp"))
                    {
                        if (compTransform)
                        {
                            // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦, Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                            // m_camera->SetTarget(compTransform->position);
                        }
                    }
                }

                // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Button("? Add New Compartment", ImVec2(-1, 0)))
                {
                    CreateNewCompartment(registry, entity);
                }

                // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                ImGui::SameLine();
                if (ImGui::Button("?? Check Integrity"))
                {


                }
            }
            else
            {
                // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦, Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                ImGui::TextColored(COL_ACCENT, "?? Compartment (linked)");
                ImGui::Spacing();


                ImGui::TextColored(COL_TEXT_DIM, "Parent Submarine Entity ID:");
                ImGui::SetNextItemWidth(-1);

                Entity parentID = subComp->submarineRoot;
                if (ImGui::DragInt("##ParentSubID", (int*)&parentID, 1.0f, 0, 10000))
                {
                    subComp->submarineRoot = parentID;
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("Set to Self"))
                {
                    subComp->submarineRoot = entity;
                }

                ImGui::Text("Current: %u", subComp->submarineRoot);
                // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                if (ImGui::Button("Unlink from Submarine", ImVec2(-1, 0)))
                {
                    subComp->submarineRoot = INVALID_ENTITY;
                    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ allCompartments Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                }

                // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ CompartmentComponent)
                auto* compData = registry.GetComponent<CompartmentComponent>(entity);
                if (compData)
                {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    ImGui::TextColored(COL_TEXT_DIM, " Local Bounds (relative to submarine)");
                    float min[3] = { compData->localMin.x, compData->localMin.y, compData->localMin.z };
                    float max[3] = { compData->localMax.x, compData->localMax.y, compData->localMax.z };

                    if (ImGui::DragFloat3("##CompMin", min, 0.1f))
                    {
                        compData->localMin = { min[0], min[1], min[2] };
                        compData->RecalculateVolume();
                    }
                    if (ImGui::DragFloat3("##CompMax", max, 0.1f))
                    {
                        compData->localMax = { max[0], max[1], max[2] };
                        compData->RecalculateVolume();
                    }

                    ImGui::Text("Volume: %.2f m?", compData->volume);
                }
            }

            // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button(" Remove Submarine Component", ImVec2(-1, 0)))
            {
                // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                if (subComp->submarineRoot == entity)
                {
                    for (Entity compEnt : subComp->allCompartments)
                    {
                        auto* link = registry.GetComponent<SubmarineComponent>(compEnt);
                        if (link) link->submarineRoot = INVALID_ENTITY;
                    }
                }
                registry.RemoveComponent<SubmarineComponent>(entity);
            }
            ImGui::PopStyleColor(2);
        }
    }


    // ===========================================================================
  // ?? RIGIDBODY COMPONENT Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
  // ===========================================================================
    if (registry.HasComponent<RigidBodyComponent>(entity))
    {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.10f, 0.20f, 0.30f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.14f, 0.28f, 0.40f, 1.0f));
        const bool jumpHere = prepareJump("rigidbody");
        bool open = ImGui::CollapsingHeader("  RigidBody Component##rbc", ImGuiTreeNodeFlags_DefaultOpen);
        finishJump(jumpHere);
        ImGui::PopStyleColor(2);

        if (open)
        {
            auto* rb = registry.GetComponent<RigidBodyComponent>(entity);
            bool needsRecreate = false;

            ImGui::Spacing();

            // ?? 1. Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ COMPOUND MODE
            if (ImGui::Checkbox(" Compound Collision", &rb->isCompound))
                rb->needsCollisionRebuild = true;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Enable to build collision from multiple primitive shapes");

            if (rb->isCompound)
            {
                // ===============================================================
                // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                // ===============================================================
                ImGui::Separator();
                ImGui::TextColored(COL_TEXT_DIM, " Compound Elements");

                // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                if (ImGui::Button("+ Add Element", ImVec2(-1, 0)))
                {
                    RigidBodyComponent::CompoundElement newElem;
                    newElem.type = RigidBodyComponent::CompoundElement::Box;
                    newElem.localPosition = { 0, 0, 0 };
                    newElem.localRotation = { 0, 0, 0, 1 };
                    newElem.size = 1.0f;
                    newElem.radius = 0.5f;
                    newElem.height = 1.0f;
                    newElem.rampSize = { 2.0f, 1.0f, 3.0f };
                    // VHACD defaults
                    newElem.vhacdModel = nullptr;
                    newElem.vhacdScale = { 1.0f, 1.0f, 1.0f };
                    newElem.vhacdResolution = 800;
                    newElem.vhacdMaxHulls = 4;
                    newElem.vhacdMaxVertsPerHull = 4;
                    newElem.vhacdVolumeError = 0.005f;
                    rb->compoundElements.push_back(newElem);
                    needsRecreate = true;
                }

                ImGui::Spacing();

                // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                for (size_t i = 0; i < rb->compoundElements.size(); ++i)
                {
                    auto& elem = rb->compoundElements[i];
                    ImGui::PushID(static_cast<int>(i));

                    char label[64];
                    sprintf_s(label, "Element %zu", i);
                    bool elemOpen = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);

                    if (elemOpen)
                    {
                        ImGui::Indent();

                        // ?? FIX #1: Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ + IM_ARRAYSIZE
                        const char* types[] = { "Box", "Sphere", "Cylinder", "Capsule", "ConvexHull", "VHACD", "StairRamp" };
                        int typeIdx = static_cast<int>(elem.type);
                        if (ImGui::Combo("Type", &typeIdx, types, IM_ARRAYSIZE(types)))
                        {
                            elem.type = static_cast<RigidBodyComponent::CompoundElement::ShapeType>(typeIdx);
                            needsRecreate = true;
                        }

                        // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                        if (ImGui::DragFloat3("Position", &elem.localPosition.x, 0.1f))
                            needsRecreate = true;

                        // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (Euler Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦)
                        if (elem.editorEulerDirty)
                        {
                            btQuaternion q(elem.localRotation.x, elem.localRotation.y, elem.localRotation.z, elem.localRotation.w);
                            btScalar yaw = 0, pitch = 0, roll = 0;
                            q.getEulerZYX(yaw, pitch, roll);
                            elem.editorEulerDegrees = {
                                static_cast<float>(roll) * (180.0f / XM_PI),
                                static_cast<float>(pitch) * (180.0f / XM_PI),
                                static_cast<float>(yaw) * (180.0f / XM_PI)
                            };
                            elem.editorEulerDirty = false;
                        }

                        if (ImGui::DragFloat3("Rotation (deg)", &elem.editorEulerDegrees.x, 0.5f, -360.0f, 360.0f, "%.1fР С—РЎвЂ”Р вЂ¦"))
                        {
                            float rx = elem.editorEulerDegrees.x * (XM_PI / 180.0f);
                            float ry = elem.editorEulerDegrees.y * (XM_PI / 180.0f);
                            float rz = elem.editorEulerDegrees.z * (XM_PI / 180.0f);
                            btQuaternion q; q.setEulerZYX(rz, ry, rx);
                            elem.localRotation = { static_cast<float>(q.x()), static_cast<float>(q.y()), static_cast<float>(q.z()), static_cast<float>(q.w()) };
                            needsRecreate = true;
                        }

                        // ?? FIX #2: meshRenderer Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                        auto* meshRenderer = registry.GetComponent<MeshRendererComponent>(entity);

                        // ===========================================================
                        // ?? VHACD-Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ VHACD)
                        // ===========================================================
                        if (elem.type == RigidBodyComponent::CompoundElement::VHACD)
                        {
                            ImGui::Spacing(); ImGui::Separator();
                            ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), " VHACD Settings");
                            ImGui::Indent();
                            bool vhacdChanged = false;

                            // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                            ImGui::Text("Source Model:"); ImGui::SameLine();
                       //    const char* modelName = (elem.vhacdModel && elem.vhacdModel->GetName) ? elem.vhacdModel->GetName() : "None";
                            ImGui::TextDisabled(" %s", "modelName");
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Pick..."))
                            {
                                if (meshRenderer && meshRenderer->model)
                                {
                                    elem.vhacdModel = meshRenderer->model;
                                    vhacdChanged = true;
                                }
                            }
                            ImGui::SameLine();

                            // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                            ImGui::Spacing(); ImGui::Text("Collision Scale:");
                            if (ImGui::DragFloat3("##VHACDScale", &elem.vhacdScale.x, 0.1f, 0.1f, 10.0f, "%.2f"))
                            {
                                elem.vhacdScale.x = fmaxf(0.1f, elem.vhacdScale.x);
                                elem.vhacdScale.y = fmaxf(0.1f, elem.vhacdScale.y);
                                elem.vhacdScale.z = fmaxf(0.1f, elem.vhacdScale.z);
                                vhacdChanged = true;
                            }
                            ImGui::SameLine(); if (ImGui::SmallButton("Reset")) { elem.vhacdScale = { 1,1,1 }; vhacdChanged = true; }

                            // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                            if (ImGui::CollapsingHeader("Quality Parameters", ImGuiTreeNodeFlags_DefaultOpen))
                            {
                                ImGui::Indent();
                                int res = static_cast<int>(elem.vhacdResolution);
                                if (ImGui::SliderInt("Resolution", &res, 10000, 400000, "%dk")) { elem.vhacdResolution = static_cast<uint32_t>(res); vhacdChanged = true; }
                                ImGui::SameLine();

                                int maxHulls = static_cast<int>(elem.vhacdMaxHulls);
                                if (ImGui::SliderInt("Max Hulls", &maxHulls, 1, 128, "%d")) { elem.vhacdMaxHulls = static_cast<uint32_t>(maxHulls); vhacdChanged = true; }
                                ImGui::SameLine();

                                int verts = static_cast<int>(elem.vhacdMaxVertsPerHull);
                                if (ImGui::SliderInt("Verts/Hull", &verts, 4, 256, "%d")) { elem.vhacdMaxVertsPerHull = static_cast<uint32_t>(verts); vhacdChanged = true; }
                                ImGui::SameLine();

                                float err = elem.vhacdVolumeError;
                                if (ImGui::SliderFloat("Volume Error %%", &err, 0.001f, 0.1f, "%.3f%%")) { elem.vhacdVolumeError = err; vhacdChanged = true; }
                                ImGui::SameLine();
                                ImGui::Unindent();
                            }

                            // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                            ImGui::Spacing();
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.7f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));
                            if (ImGui::Button(" Regenerate Collision", ImVec2(-1, 0)))
                            {
                                if (elem.vhacdModel) { needsRecreate = true; }
                                else { ImGui::SetTooltip("?? Assign a model first!"); }
                            }
                            ImGui::PopStyleColor(2);

                            if (elem.vhacdLastHullCount > 0)
                                ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "? Generated: %d convex hulls", elem.vhacdLastHullCount);

                            ImGui::Unindent();
                            if (vhacdChanged) needsRecreate = true;
                        }
                        // ===========================================================
                        // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ VHACD-Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                        // ===========================================================

                        // ?? FIX #3: Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ VHACD Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ else
                        bool sizeChanged = false;
                        if (elem.type == RigidBodyComponent::CompoundElement::Box)
                        {
                            if (ImGui::DragFloat("Size", &elem.size, 0.1f, 0.1f, 10.0f)) sizeChanged = true;
                        }
                        else if (elem.type == RigidBodyComponent::CompoundElement::Sphere)
                        {
                            if (ImGui::DragFloat("Radius", &elem.radius, 0.1f, 0.1f, 10.0f)) sizeChanged = true;
                        }
                        else if (elem.type == RigidBodyComponent::CompoundElement::Cylinder ||
                            elem.type == RigidBodyComponent::CompoundElement::Capsule)
                        {
                            if (ImGui::DragFloat("Radius", &elem.radius, 0.1f, 0.1f, 10.0f)) sizeChanged = true;
                            if (ImGui::DragFloat("Height", &elem.height, 0.1f, 0.1f, 10.0f)) sizeChanged = true;
                        }
                        else if (elem.type == RigidBodyComponent::CompoundElement::StairRamp)
                        {
                            ImGui::TextColored(COL_TEXT_DIM, "Ramp Size: X width, Y climb height, Z stair length");
                            if (ImGui::DragFloat3("##CompoundStairRampSize", &elem.rampSize.x, 0.05f, 0.01f, 100.0f)) sizeChanged = true;
                        }
                        // VHACD, ConvexHull Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦!

                        if (sizeChanged) needsRecreate = true;

                        // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                        ImGui::Spacing();
                        if (ImGui::Button("Delete", ImVec2(-1, 0)))
                        {
                            rb->compoundElements.erase(rb->compoundElements.begin() + i);
                            needsRecreate = true;
                            ImGui::PopID();
                            break;
                        }

                        // Advanced Offsets (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Box / Cylinder / Capsule)
                        ImGui::Spacing(); ImGui::Separator();
                        ImGui::TextColored(COL_TEXT_DIM, " Advanced Size & Offsets");
                        ImGui::Indent();
                        bool shapeChanged = false;

                        if (elem.type == RigidBodyComponent::CompoundElement::Box)
                        {
                            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), " Half-Extents Adjustment");
                            if (ImGui::DragFloat3(" Min Offset (Neg)", &elem.boxOffsetMin.x, 0.05f, -100.0f, 100.0f)) shapeChanged = true;
                            ImGui::SameLine();
                            if (ImGui::DragFloat3(" Max Offset (Pos)", &elem.boxOffsetMax.x, 0.05f, -100.0f, 100.0f)) shapeChanged = true;
                            ImGui::SameLine();

                            // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Corner Edit (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦)
                            ImGui::Spacing();
                            bool isThisElemActive = (m_cornerEditActive && m_cornerEditEntity == entity && m_cornerEditElemIndex == (int)i);
                            ImGui::PushStyleColor(ImGuiCol_Button, isThisElemActive ? ImVec4(0.8f, 0.4f, 0.0f, 1) : ImVec4(0.22f, 0.22f, 0.22f, 1));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, isThisElemActive ? ImVec4(1.0f, 0.55f, 0.1f, 1) : ImVec4(0.32f, 0.32f, 0.32f, 1));
                            if (ImGui::Button(isThisElemActive ? "Corner Edit: ON" : "Edit Corners in Viewport", ImVec2(-1, 0)))
                            {
                                if (isThisElemActive) { m_cornerEditActive = false; m_cornerEditEntity = INVALID_ENTITY; m_cornerEditElemIndex = -1; }
                                else { m_cornerEditActive = true; m_cornerEditEntity = entity; m_cornerEditElemIndex = (int)i; }
                            }
                            ImGui::PopStyleColor(2);
                            if (isThisElemActive) ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "  Drag white dots in viewport");
                        }
                        else if (elem.type == RigidBodyComponent::CompoundElement::Cylinder ||
                            elem.type == RigidBodyComponent::CompoundElement::Capsule)
                        {
                            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), " Dimension Offsets");
                            float ro = elem.radiusOffset;
                            if (ImGui::DragFloat(" Radius Offset", &ro, 0.05f, -0.5f, 0.5f)) { elem.radiusOffset = ro; shapeChanged = true; }
                            float hot = elem.heightOffsetTop;
                            if (ImGui::DragFloat(" Top Height Offset", &hot, 0.05f, -1.0f, 1.0f)) { elem.heightOffsetTop = hot; shapeChanged = true; }
                            float hob = elem.heightOffsetBottom;
                            if (ImGui::DragFloat(" Bottom Height Offset", &hob, 0.05f, -1.0f, 1.0f)) { elem.heightOffsetBottom = hob; shapeChanged = true; }
                        }

                        ImGui::Unindent();
                        if (shapeChanged) needsRecreate = true;
                        ImGui::Unindent();
                    }
                    ImGui::PopID();
                }

                if (needsRecreate)
                    rb->needsCollisionRebuild = true;
                needsRecreate = false;  // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦: Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦!
            }
            else
            {
                // ===============================================================
                // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                // ===============================================================
                ImGui::Separator();
                ImGui::TextColored(COL_TEXT_DIM, " Mass");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##RBMass", &rb->mass, 0.1f, 0.0f, 10000.0f))
                    if (rb->body) PhysicsSystem::RefreshRigidBody(registry, entity, rb);

                ImGui::Spacing();
                ImGui::TextColored(COL_TEXT_DIM, " Shape Type");

                // ?? FIX #4: Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ + IM_ARRAYSIZE Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                const char* shapeNames[] = {
                    "Box", "Sphere", "Cylinder", "Capsule",
                    "Plane", "ConvexHull", "TriangleMesh", "GImpactMesh", "VHACD", "StairRamp"
                };
                int shapeIdx = static_cast<int>(rb->shapeType);
                if (ImGui::Combo("##RBShape", &shapeIdx, shapeNames, IM_ARRAYSIZE(shapeNames)))
                {
                    rb->shapeType = static_cast<RigidBodyComponent::ShapeType>(shapeIdx);
                    if (rb->body)  PhysicsSystem::RefreshRigidBody(registry, entity, rb);
                }

                ImGui::Spacing();

                // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                switch (rb->shapeType)
                {
                case RigidBodyComponent::Sphere:
                {
                    auto* meshRenderer = registry.GetComponent<MeshRendererComponent>(entity);
                    if (ImGui::Checkbox(" Manual Size##SphereManual", &rb->useManualSize))
                        if (rb->body)  PhysicsSystem::RefreshRigidBody(registry, entity, rb);
                    ImGui::SameLine(); ImGui::TextColored(COL_TEXT_DIM, "(ignore mesh bounds)");
                    if (rb->useManualSize)
                    {
                        ImGui::Spacing(); ImGui::TextColored(COL_TEXT_DIM, " Radius");
                        ImGui::SetNextItemWidth(-1);
                        float radius = rb->shapeRadius;
                        if (ImGui::DragFloat("##Radius", &radius, 0.05f, 0.01f, 100.0f))
                        {
                            rb->shapeRadius = radius; if (rb->body) PhysicsSystem::RefreshRigidBody(registry, entity, rb);
                        }
                    }
                    else
                    {
                        if (meshRenderer && meshRenderer->model)
                        {
                            XMFLOAT3 center; float meshRadius;
                            meshRenderer->model->GetBoundingSphere(&center, &meshRadius);
                            rb->shapeRadius = meshRadius;
                            ImGui::TextColored(COL_TEXT_DIM, " Auto radius from mesh: %.3f", meshRadius);
                        }
                        else
                            ImGui::TextColored(COL_TEXT_DIM, " No mesh - using shapeRadius: %.3f", rb->shapeRadius);
                    }
                    break;
                }
                case RigidBodyComponent::Box:
                {
                    auto* meshRenderer = registry.GetComponent<MeshRendererComponent>(entity);
                    if (!rb->useManualSize && meshRenderer && meshRenderer->model)
                    {
                        XMFLOAT3 min, max; meshRenderer->model->GetBoundingBox(&min, &max);
                        rb->shapeSize = btVector3((max.x - min.x) * 0.5f, (max.y - min.y) * 0.5f, (max.z - min.z) * 0.5f);
                        ImGui::TextColored(COL_TEXT_DIM, " Auto box from mesh: (%.2f, %.2f, %.2f)", rb->shapeSize.x(), rb->shapeSize.y(), rb->shapeSize.z());
                    }
                    ImGui::TextColored(COL_TEXT_DIM, " Half Extents (X, Y, Z)");
                    float size[3] = { rb->shapeSize.x(), rb->shapeSize.y(), rb->shapeSize.z() };
                    if (ImGui::DragFloat3("##BoxSize", size, 0.05f, 0.01f, 100.0f))
                    {
                        rb->shapeSize = btVector3(size[0], size[1], size[2]); rb->useManualSize = true; if (rb->body)  PhysicsSystem::RefreshRigidBody(registry, entity, rb);
                    }
                    break;
                }
                case RigidBodyComponent::Cylinder:
                {
                    float radius = rb->shapeRadius, height = rb->shapeSize.y(); bool changed = false;
                    ImGui::TextColored(COL_TEXT_DIM, " Radius"); ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##CylRadius", &radius, 0.05f, 0.01f, 100.0f)) { rb->shapeRadius = radius; changed = true; }
                    ImGui::Spacing(); ImGui::TextColored(COL_TEXT_DIM, " Height"); ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##CylHeight", &height, 0.05f, 0.01f, 100.0f)) { rb->shapeSize.setY(height); changed = true; }
                    if (changed && rb->body) PhysicsSystem::RefreshRigidBody(registry, entity, rb);
                    break;
                }
                case RigidBodyComponent::Capsule:
                {
                    float radius = rb->shapeRadius, height = rb->shapeSize.y(); bool changed = false;
                    ImGui::TextColored(COL_TEXT_DIM, " Radius"); ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##CapsRadius", &radius, 0.05f, 0.01f, 100.0f)) { rb->shapeRadius = radius; changed = true; }
                    ImGui::Spacing(); ImGui::TextColored(COL_TEXT_DIM, " Height"); ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##CapsHeight", &height, 0.05f, 0.01f, 100.0f)) { rb->shapeSize.setY(height); changed = true; }
                    if (changed && rb->body)  PhysicsSystem::RefreshRigidBody(registry, entity, rb);
                    break;
                }
                case RigidBodyComponent::ConvexHull:
                {
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), " Auto-calculated from mesh geometry");
                    auto* meshRenderer = registry.GetComponent<MeshRendererComponent>(entity);
                    if (!meshRenderer || !meshRenderer->model) ImGui::TextColored(COL_RED, "?? No mesh assigned");
                    break;
                }
                case RigidBodyComponent::TriangleMesh:
                {
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), " Exact mesh collision (static only)");
                    rb->mass = 0.0f;
                    break;
                }
                case RigidBodyComponent::GImpactMesh:
                {
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), " Exact mesh collision (dynamic!)");
                    ImGui::TextColored(COL_TEXT_DIM, " Supports mass > 0, objects can be inside.");
                    auto* meshRenderer = registry.GetComponent<MeshRendererComponent>(entity);
                    if (meshRenderer && meshRenderer->model)
                    {
                        std::vector<XMFLOAT3> tris; meshRenderer->model->GetRawTriangles(tris);
                        ImGui::TextColored(COL_TEXT_DIM, " Triangles: %zu", tris.size() / 3);
                        if (tris.size() / 3 > 5000) ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), " ? High triangle count!");
                    }
                    else ImGui::TextColored(COL_RED, "?? No mesh assigned!");
                    break;
                }
                // ?? FIX #5: Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ VHACD Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                case RigidBodyComponent::StairRamp:
                {
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), " Smooth player stair collision");
                    ImGui::TextColored(COL_TEXT_DIM, " Size: X width, Y climb height, Z stair length");
                    float size[3] = { rb->shapeSize.x(), rb->shapeSize.y(), rb->shapeSize.z() };
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat3("##StairRampSize", size, 0.05f, 0.01f, 100.0f))
                    {
                        rb->shapeSize = btVector3(size[0], size[1], size[2]);
                        if (rb->body) PhysicsSystem::RefreshRigidBody(registry, entity, rb);
                    }
                    rb->mass = 0.0f;
                    break;
                }                case RigidBodyComponent::VHACD:
                {
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), " VHACD: Dynamic concave collision");
                    ImGui::TextColored(COL_TEXT_DIM, " Decomposes mesh into convex hulls for stable physics.");
                    auto* meshRenderer = registry.GetComponent<MeshRendererComponent>(entity);
                    if (meshRenderer && meshRenderer->model)
                    {
                        std::vector<XMFLOAT3> tris; meshRenderer->model->GetRawTriangles(tris);
                        ImGui::TextColored(COL_TEXT_DIM, " Triangles: %zu Р С—РЎвЂ”Р вЂ¦ will be decomposed", tris.size() / 3);
                    }
                    else ImGui::TextColored(COL_RED, "?? Assign a mesh first!");
                    break;
                }
                case RigidBodyComponent::Plane:
                {
                    ImGui::TextColored(COL_TEXT_DIM, " Y"); ImGui::SetNextItemWidth(-1);
                    float y = rb->shapeSize.y();
                    if (ImGui::DragFloat("##Y", &y, 0.05f, 0.01f, 100.0f)) { rb->shapeSize.setY(y); if (rb->body)  PhysicsSystem::RefreshRigidBody(registry, entity, rb);
                    }
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), " Infinite plane at Y=0");
                    rb->mass = 0.0f;
                    break;
                }
                }

                ImGui::Spacing();
                ImGui::Checkbox(" Enable CCD##RB", &rb->enableCCD);
                // ?? COLLISION FILTERING
// ===============================================================
               
            }

            const bool canApplyRbRebuild =
                rb->needsCollisionRebuild &&
                !ImGui::IsAnyItemActive() &&
                !ImGui::IsMouseDown(ImGuiMouseButton_Left);

            if (canApplyRbRebuild)
            {
                if (rb->body)
                    PhysicsSystem::RefreshRigidBody(registry, entity, rb);
                else
                    PhysicsSystem::CreateRigidBody(registry, entity);

                rb->needsCollisionRebuild = false;
            }
        }
    }

	
    if (registry.HasComponent<BuoyancyComponent>(entity))
    {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.10f, 0.30f, 0.20f, 1.0f)); // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.14f, 0.40f, 0.28f, 1.0f));
        const bool jumpHere = prepareJump("buoyancy");
        bool open = ImGui::CollapsingHeader("  Buoyancy Component##buoy", ImGuiTreeNodeFlags_DefaultOpen);
        finishJump(jumpHere);
        ImGui::PopStyleColor(2);

        if (open)
        {
            auto* buoy = registry.GetComponent<BuoyancyComponent>(entity);
            if (!buoy) { ImGui::TextColored(COL_RED, "Component error"); return; }

            ImGui::Spacing();

            // -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ -------------------------------------
            ImGui::TextColored(COL_TEXT_DIM, " Fluid Density (kg/m?)");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##FluidDensity", &buoy->fluidDensity, 10.0f, 0.0f, 5000.0f, "%.1f"))
            {
                // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦, Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset##Density")) buoy->fluidDensity = 1000.0f;
            ImGui::TextColored(COL_TEXT_DIM, "  Water: 1000, Sea: 1025, Oil: ~900");

            ImGui::Spacing();

            // -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ ------------------------------------------
            ImGui::TextColored(COL_TEXT_DIM, " Volume (m?)");
            ImGui::SetNextItemWidth(-1);
            float oldVolume = buoy->volume;
            if (ImGui::DragFloat("##Volume", &buoy->volume, 0.05f, 0.01f, 1000.0f, "%.2f"))
            {
                // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Auto##Volume"))
            {
                // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦-Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦)
                auto* mesh = registry.GetComponent<MeshRendererComponent>(entity);
                if (mesh && mesh->model)
                {
                    XMFLOAT3 min, max;
                    mesh->model->GetBoundingBox(&min, &max);
                    buoy->volume = (max.x - min.x) * (max.y - min.y) * (max.z - min.z);
                    buoy->height = max.y - min.y; // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                }
            }

            ImGui::Spacing();

            // -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ % Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦) ---------------
            ImGui::TextColored(COL_TEXT_DIM, " Height (m) Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##Height", &buoy->height, 0.05f, 0.01f, 100.0f, "%.2f"))
            {
                // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ submersionRatio
            }

            ImGui::Spacing();

            // -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦) -----------------
            ImGui::TextColored(COL_TEXT_DIM, " Buoyancy Multiplier (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦)");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("##BuoyMult", &buoy->buoyancyMultiplier, 0.1f, 2.0f, "%.2f"))
            {
                // <1.0 = Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦, >1.0 = Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset##Mult")) buoy->buoyancyMultiplier = 1.0f;
            ImGui::TextColored(COL_TEXT_DIM, "  0.5 = Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦, 1.0 = Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦, 1.5 = Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦) ------------------
            ImGui::TextColored(COL_TEXT_DIM, " Debug Info (read-only)");
            ImGui::Indent();
            ImGui::Text("Smoothed Submersion: %.3f", buoy->smoothedSubmersion);
            // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦, Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ PhysicsSystem
            ImGui::Unindent();

            ImGui::Spacing();

            // -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ -----------------------------
            ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button(" Remove Buoyancy Component", ImVec2(-1, 0)))
            {
                registry.RemoveComponent<BuoyancyComponent>(entity);
                // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Bullet
            }
            ImGui::PopStyleColor(2);
        }
    }


    // -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ "+ Add Component" ----------------------------------------------
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
    if (ImGui::Button("  + Add Component", ImVec2(-1, 0)))
        ImGui::OpenPopup("##AddComponentPopup");


    ImGui::PopStyleColor(2);

    if (ImGui::BeginPopup("##AddComponentPopup"))
    {
        ImGui::TextColored(COL_TEXT_DIM, "Available Components");
        ImGui::SetNextItemWidth(300.0f);
        ImGui::InputTextWithHint(
            "##ComponentSearch", "Search components...",
            m_componentSearch, sizeof(m_componentSearch));
        ImGui::Separator();
        ImGui::Spacing();

        bool anyComponentMatches = false;
        auto componentMatches = [&](const char* componentName)
        {
            const bool matches = ContainsCaseInsensitive(componentName, m_componentSearch);
            anyComponentMatches |= matches;
            return matches;
        };

        if (componentMatches("NPC AI Components"))
        {
            const bool hasNpc = registry.HasComponent<NPCComponent>(entity);
            ImGui::BeginDisabled(hasNpc);
            if (ImGui::Selectable("  NPC AI Components"))
            {
                registry.AddComponent<NPCComponent>(entity, NPCComponent{});
                if (!registry.HasComponent<CharacterVitalsComponent>(entity))
                    registry.AddComponent<CharacterVitalsComponent>(
                        entity, CharacterVitalsComponent{});
                registry.AddComponent<NpcBrainComponent>(entity, NpcBrainComponent{});
                registry.AddComponent<NPCMovementComponent>(entity, NPCMovementComponent{});
                registry.AddComponent<NpcPatrolComponent>(entity, NpcPatrolComponent{});
                if (!registry.HasComponent<CharacterMovement>(entity))
                    registry.AddComponent<CharacterMovement>(entity, CharacterMovement{});
                if (!registry.HasComponent<CharacterController>(entity))
                    registry.AddComponent<CharacterController>(entity, CharacterController{});
                if (!registry.HasComponent<CharacterPhysicsTuningComponent>(entity))
                    registry.AddComponent<CharacterPhysicsTuningComponent>(
                        entity, CharacterPhysicsTuningComponent{});
                if (!registry.HasComponent<AnimationComponent>(entity))
                    registry.AddComponent<AnimationComponent>(entity, AnimationComponent{});
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            if (hasNpc)
            {
                ImGui::SameLine();
                ImGui::TextColored(COL_TEXT_DIM, "(already added)");
            }


        }
        if (componentMatches("Faction Component"))
        {
            // Фракция нужна любой сущности, которая участвует в отношениях
            // фракций: как обычному NPC, так и управляемому игроку.
            const bool isCombatant =
                registry.HasComponent<NPCComponent>(entity) ||
                registry.HasComponent<PlayerGameTag>(entity);

            const bool hasFaction =
                registry.HasComponent<FactionComponent>(entity);

            // Не разрешаем добавлять фракцию произвольным объектам уровня
            // и не добавляем второй экземпляр уже существующего компонента.
            ImGui::BeginDisabled(!isCombatant || hasFaction);

            if (ImGui::Selectable("  Faction Component"))
            {
                registry.AddComponent<FactionComponent>(
                    entity,
                    FactionComponent{});

                ImGui::CloseCurrentPopup();
            }

            ImGui::EndDisabled();

            if (!isCombatant)
            {
                ImGui::SameLine();
                ImGui::TextColored(
                    COL_TEXT_DIM,
                    "(requires NPC AI or Player tag)");
            }
            else if (hasFaction)
            {
                ImGui::SameLine();
                ImGui::TextColored(COL_TEXT_DIM, "(already added)");
            }
        }


        if (componentMatches("NPC Combat Component"))
        {
            const bool hasNpc =
                registry.HasComponent<NPCComponent>(entity);

            const bool hasCombat =
                registry.HasComponent<NPCCombatComponent>(entity);

            ImGui::BeginDisabled(!hasNpc || hasCombat);

            if (ImGui::Selectable("  NPC Combat Component"))
            {
                registry.AddComponent<NPCCombatComponent>(
                    entity, NPCCombatComponent{});
                if (!registry.HasComponent<NpcCombatModulesComponent>(entity))
                {
                    NpcCombatModulesComponent modules;
                    modules.ApplyPreset(NpcCombatModulePreset::MinimalDuel);
                    registry.AddComponent<NpcCombatModulesComponent>(
                        entity, modules);
                }

                ImGui::CloseCurrentPopup();
            }

            ImGui::EndDisabled();

            if (!hasNpc)
            {
                ImGui::SameLine();
                ImGui::TextColored(COL_TEXT_DIM, "(requires NPC AI)");
            }
            else if (hasCombat)
            {
                ImGui::SameLine();
                ImGui::TextColored(COL_TEXT_DIM, "(already added)");
            }
        }
        if (componentMatches("NPC Combat Tactics"))
        {
            const bool hasNpc = registry.HasComponent<NPCComponent>(entity);
            const bool exists = registry.HasComponent<NpcTacticalComponent>(entity);
            ImGui::BeginDisabled(!hasNpc || exists);
            if (ImGui::Selectable("  NPC Combat Tactics"))
            {
                registry.AddComponent<NpcTacticalComponent>(entity, NpcTacticalComponent{});
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
        }
        if (componentMatches("NPC Cover Point"))
        {
            const bool exists = registry.HasComponent<NpcCoverPointComponent>(entity);
            ImGui::BeginDisabled(exists);
            if (ImGui::Selectable("  NPC Cover Point"))
            {
                registry.AddComponent<NpcCoverPointComponent>(entity, NpcCoverPointComponent{});
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
        }
        if (componentMatches("NPC Patrol Point"))
        {
            const bool exists = registry.HasComponent<NpcPatrolPointComponent>(entity);
            ImGui::BeginDisabled(exists);
            if (ImGui::Selectable("  NPC Patrol Point"))
            {
                registry.AddComponent<NpcPatrolPointComponent>(
                    entity, NpcPatrolPointComponent{});
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
        }


        // -- Submarine Component --------------------------------------------------
        if (componentMatches("Submarine Component"))
        {
            const bool hasSub = registry.HasComponent<SubmarineComponent>(entity);
            ImGui::BeginDisabled(hasSub);
            if (ImGui::Selectable("  Submarine Component"))
            {
                SubmarineComponent sub{};
                sub.submarineRoot = entity;
                registry.AddComponent<SubmarineComponent>(entity, sub);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            if (hasSub)
            {
                ImGui::SameLine();
                ImGui::TextColored(COL_TEXT_DIM, "(already added)");
            }
        }

        if (componentMatches("Player GameTag"))
        {
            const bool hasPlayer = registry.HasComponent<PlayerGameTag>(entity);
            ImGui::BeginDisabled(hasPlayer);
            if (ImGui::Selectable("  Player GameTag"))
            {
                auto& tags = registry.GetComponents<PlayerGameTag>();
                std::vector<Entity> oldPlayers;
                for (auto& [candidate, tag] : tags)
                {
                    if (candidate != entity)
                        oldPlayers.push_back(candidate);
                }
                for (Entity oldPlayer : oldPlayers)
                    registry.RemoveComponent<PlayerGameTag>(oldPlayer);

                registry.AddComponent<PlayerGameTag>(entity, PlayerGameTag{});
                if (!registry.HasComponent<PlayerComponent>(entity))
                    registry.AddComponent<PlayerComponent>(entity, PlayerComponent{});
                if (!registry.HasComponent<CharacterVitalsComponent>(entity))
                    registry.AddComponent<CharacterVitalsComponent>(
                        entity, CharacterVitalsComponent{});
                if (!registry.HasComponent<CharacterMovement>(entity))
                    registry.AddComponent<CharacterMovement>(entity, CharacterMovement{});
                if (!registry.HasComponent<CharacterController>(entity))
                    registry.AddComponent<CharacterController>(entity, CharacterController{});
                if (!registry.HasComponent<CharacterPhysicsTuningComponent>(entity))
                    registry.AddComponent<CharacterPhysicsTuningComponent>(
                        entity, CharacterPhysicsTuningComponent{});
                if (!registry.HasComponent<AnimationComponent>(entity))
                    registry.AddComponent<AnimationComponent>(entity, AnimationComponent{});

                if (!registry.HasComponent<EquippedWeaponComponent>(entity))
                {
                    Entity weaponEntity = registry.CreateEntity();
                    registry.AddComponent<WeaponComponent>(weaponEntity, WeaponComponent{});
                    registry.AddComponent<WeaponOwnerComponent>(weaponEntity, WeaponOwnerComponent{ entity });
                    registry.AddComponent<ActiveComponent>(weaponEntity, ActiveComponent{});
                    registry.AddComponent<EquippedWeaponComponent>(
                        entity,
                        EquippedWeaponComponent{ weaponEntity });
                }
                InventorySystem::EnsureInventory(registry, entity);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
        }

        if (componentMatches("Weapon Component"))
        {
            const bool weaponHas = registry.HasComponent<WeaponComponent>(entity);
            ImGui::BeginDisabled(weaponHas);
            if (ImGui::Selectable("  Weapon Component"))
            {
                registry.AddComponent<WeaponComponent>(entity, WeaponComponent{});
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
        }

        if (componentMatches("First Person View"))
        {
            const bool hasFirstPerson =
                registry.HasComponent<FirstPersonViewComponent>(entity);
            ImGui::BeginDisabled(hasFirstPerson);
            if (ImGui::Selectable("  First Person View"))
            {
                Entity player = INVALID_ENTITY;
                registry.ForEach<PlayerGameTag>(
                    [&](Entity candidate, PlayerGameTag&)
                    {
                        if (player == INVALID_ENTITY)
                            player = candidate;
                    });

                FirstPersonViewComponent viewModel;
                viewModel.owner = player;
                registry.AddComponent<FirstPersonViewComponent>(entity, viewModel);
                if (!registry.HasComponent<AnimationComponent>(entity))
                    registry.AddComponent<AnimationComponent>(entity, AnimationComponent{});
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
        }

        if (componentMatches("Bone Attachment"))
        {
            const bool hasAttachment =
                registry.HasComponent<BoneAttachmentComponent>(entity);
            ImGui::BeginDisabled(hasAttachment);
            if (ImGui::Selectable("  Bone Attachment"))
            {
                registry.AddComponent<BoneAttachmentComponent>(
                    entity, BoneAttachmentComponent{});
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
        }

        if (componentMatches("Interaction Component"))
        {
            const bool hasInteraction = registry.HasComponent<InteractionComponent>(entity);
            ImGui::BeginDisabled(hasInteraction);
            if (ImGui::Selectable("  Interaction Component"))
            {
                registry.AddComponent<InteractionComponent>(entity, InteractionComponent{});
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
        }

        if (componentMatches("Crew Member Component"))
        {
            const bool hasCrew = registry.HasComponent<CrewMemberComponent>(entity);
            ImGui::BeginDisabled(hasCrew);
            if (ImGui::Selectable("  Crew Member Component"))
            {
                registry.AddComponent<CrewMemberComponent>(entity, CrewMemberComponent{});
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
        }

        if (componentMatches("Inventory Component"))
        {
            const bool hasInventory = registry.HasComponent<InventoryComponent>(entity);
            ImGui::BeginDisabled(hasInventory);
            if (ImGui::Selectable("  Inventory Component"))
            {
                registry.AddComponent<InventoryComponent>(entity, InventoryComponent{});
                InventorySystem::EnsureInventory(registry, entity);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
        }

        if (componentMatches("Weapon Pickup Component"))
        {
            const bool hasPickup = registry.HasComponent<WeaponPickupComponent>(entity);
            ImGui::BeginDisabled(hasPickup);
            if (ImGui::Selectable("  Weapon Pickup Component"))
            {
                registry.AddComponent<WeaponPickupComponent>(
                    entity, WeaponPickupComponent{});
                if (!registry.HasComponent<WeaponComponent>(entity))
                    registry.AddComponent<WeaponComponent>(entity, WeaponComponent{});
                if (!registry.HasComponent<InteractionComponent>(entity))
                {
                    InteractionComponent interaction;
                    interaction.maxDistance = 3.0f;
                    interaction.prompt = "Pick up rifle";
                    registry.AddComponent<InteractionComponent>(
                        entity, std::move(interaction));
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
        }

        if (componentMatches("Room Component"))
        {
            const bool hasRoom = registry.HasComponent<RoomComponent>(entity);
            ImGui::BeginDisabled(hasRoom);
            if (ImGui::Selectable("  Room Component"))
            {
                RoomComponent room{};
                if (auto* name = registry.GetComponent<NameComponent>(entity))
                    room.roomName = name->name;
                registry.AddComponent<RoomComponent>(entity, std::move(room));
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
        }
              


        if (componentMatches("NavMesh Surface"))
        {
            const bool hasSurface = registry.HasComponent<NavMeshSurfaceComponent>(entity);
            ImGui::BeginDisabled(hasSurface);
            if (ImGui::Selectable("  NavMesh Surface"))
            {
                registry.AddComponent<NavMeshSurfaceComponent>(
                    entity, NavMeshSurfaceComponent{});
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
        }

        if (componentMatches("Collision Component"))
        {
            const bool coll = registry.HasComponent<SubmarineCollisionSetup>(entity);
            ImGui::BeginDisabled(coll);
            if (ImGui::Selectable("  Collison Component"))
            {
                SubmarineCollisionSetup collision{};
                registry.AddComponent<SubmarineCollisionSetup>(entity, collision);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            if (coll)
            {
                ImGui::SameLine();
                ImGui::TextColored(COL_TEXT_DIM, "(already added)");
            }
        }

        if (componentMatches("Door Component"))
        {
            const bool hasDoor = registry.HasComponent<DoorComponent>(entity);
            ImGui::BeginDisabled(hasDoor);
            if (ImGui::Selectable("  Door Component"))
            {
                registry.AddComponent<DoorComponent>(entity, DoorComponent{});
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            if (hasDoor)
            {
                ImGui::SameLine();
                ImGui::TextColored(COL_TEXT_DIM, "(already added)");
            }
        }

        if (componentMatches("Damageable Component"))
        {
            const bool hasDamage = registry.HasComponent<DamageableComponent>(entity);
            ImGui::BeginDisabled(hasDamage);
            if (ImGui::Selectable("  Damageable Component"))
            {
                registry.AddComponent<DamageableComponent>(entity, DamageableComponent{});
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
        }

        if (componentMatches("Character Vitals Component"))
        {
            const bool hasVitals =
                registry.HasComponent<CharacterVitalsComponent>(entity);
            ImGui::BeginDisabled(hasVitals);
            if (ImGui::Selectable("  Character Vitals Component"))
            {
                registry.AddComponent<CharacterVitalsComponent>(
                    entity, CharacterVitalsComponent{});
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            if (hasVitals)
            {
                ImGui::SameLine();
                ImGui::TextColored(COL_TEXT_DIM, "(already added)");
            }
        }

        // -- Light Component -----------------------------------------------------
        if (componentMatches("Light Component"))
        {
            const bool hasLight = registry.HasComponent<LightComponent>(entity);
            ImGui::BeginDisabled(hasLight);
            if (ImGui::Selectable("  Light Component"))
            {
                registry.AddComponent<LightComponent>(entity, LightComponent{});
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            if (hasLight)
            {
                ImGui::SameLine();
                ImGui::TextColored(COL_TEXT_DIM, "(already added)");
            }
        }

        // -- RigidBody Component -------------------------------------------------
        if (componentMatches("RigidBody Physics Component"))
        {
            const bool hasRb = registry.HasComponent<RigidBodyComponent>(entity);
            ImGui::BeginDisabled(hasRb);
            if (ImGui::Selectable("  RigidBody Component"))
            {
                registry.AddComponent<RigidBodyComponent>(entity, RigidBodyComponent{});
                auto* rb = registry.GetComponent<RigidBodyComponent>(entity);
                if (rb && !rb->body)
                    PhysicsSystem::CreateRigidBody(registry, entity);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            if (hasRb)
            {
                ImGui::SameLine();
                ImGui::TextColored(COL_TEXT_DIM, "(already added)");
            }
        }

        // -- Buoyancy Component --------------------------------------------------
        if (componentMatches("Buoyancy Component"))
        {
            const bool hasBuoy = registry.HasComponent<BuoyancyComponent>(entity);
            ImGui::BeginDisabled(hasBuoy);
            if (ImGui::Selectable("  Buoyancy Component"))
            {
                registry.AddComponent<BuoyancyComponent>(entity, BuoyancyComponent{});
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            if (hasBuoy)
            {
                ImGui::SameLine();
                ImGui::TextColored(COL_TEXT_DIM, "(already added)");
            }
        }
        if (componentMatches("Trigger Zone Component"))
        {
            const bool hasTrigger = registry.HasComponent<TriggerZoneComponent>(entity);
            ImGui::BeginDisabled(hasTrigger);
            if (ImGui::Selectable("  Trigger Zone Component"))
            {
                if (!registry.HasComponent<TransformComponent>(entity))
                {
                    TransformComponent transform{};
                    if (auto* area = registry.GetComponent<WorldAreaComponent>(entity))
                        transform.position = area->center;
                    registry.AddComponent<TransformComponent>(entity, transform);
                }
                registry.AddComponent<TriggerZoneComponent>(entity, TriggerZoneComponent{});
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
        }
        if (componentMatches("Trigger Action Component"))
        {
            const bool hasAction = registry.HasComponent<TriggerActionComponent>(entity);
            ImGui::BeginDisabled(hasAction);
            if (ImGui::Selectable("  Trigger Action Component"))
            {
                registry.AddComponent<TriggerActionComponent>(
                    entity, TriggerActionComponent{});
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
        }
        if (componentMatches("World Area Component"))
        {
            const bool hasArea = registry.HasComponent<WorldAreaComponent>(entity);
            ImGui::BeginDisabled(hasArea);
            if (ImGui::Selectable("  World Area Component"))
            {
                registry.AddComponent<WorldAreaComponent>(entity, WorldAreaComponent{});
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            if (hasArea)
            {
                ImGui::SameLine();
                ImGui::TextColored(COL_TEXT_DIM, "(already added)");
            }
        }
        // -- Compartment Component -----------------------------------------------
        if (componentMatches("Compartment Component"))
        {
            const bool hasComp = registry.HasComponent<CompartmentComponent>(entity);
            ImGui::BeginDisabled(hasComp);
            if (ImGui::Selectable("  Compartment Component"))
            {
                CompartmentComponent comp{};
                comp.RecalculateVolume();
                registry.AddComponent<CompartmentComponent>(entity, comp);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            if (hasComp)
            {
                ImGui::SameLine();
                ImGui::TextColored(COL_TEXT_DIM, "(already added)");
            }
        }

        if (!anyComponentMatches)
            ImGui::TextDisabled("No components match this search.");

        ImGui::Spacing();
        ImGui::EndPopup();
    }
}

void EditorUI::RenderCameraPanel(SceneManager& sceneManager)
{
    SectionHeader("Play Camera");

    if (!m_cameraController)
    {
        ImGui::TextColored(COL_TEXT_DIM, "Camera controller is not available");
        return;
    }

    auto& registry = sceneManager.GetECSRegistry();
    Entity player = INVALID_ENTITY;
    registry.ForEach<PlayerGameTag>([&](Entity entity, PlayerGameTag&)
    {
        if (player == INVALID_ENTITY) player = entity;
    });
    if (player != INVALID_ENTITY &&
        !registry.HasComponent<PlayCameraComponent>(player))
        registry.AddComponent<PlayCameraComponent>(player, PlayCameraComponent{});
    auto* playCamera = registry.GetComponent<PlayCameraComponent>(player);

    const char* viewModes[] = { "Third Person", "First Person" };
    int viewMode = m_cameraController->GetPlayViewMode() == CameraController::PlayViewMode::FirstPerson ? 1 : 0;
    if (ImGui::Combo("Play View", &viewMode, viewModes, IM_ARRAYSIZE(viewModes)))
    {
        m_cameraController->SetPlayViewMode(
            viewMode == 1 ? CameraController::PlayViewMode::FirstPerson : CameraController::PlayViewMode::ThirdPerson
        );
    }

    float eyeRight = playCamera
        ? playCamera->firstPersonEyeOffset.x
        : m_cameraController->GetFirstPersonEyeRightOffset();
    if (ImGui::DragFloat("Eye Right", &eyeRight, 0.01f, -1.0f, 1.0f, "%.2f"))
    {
        if (playCamera) playCamera->firstPersonEyeOffset.x = eyeRight;
        m_cameraController->SetFirstPersonEyeRightOffset(eyeRight);
    }
    float eyeHeight = playCamera
        ? playCamera->firstPersonEyeOffset.y
        : m_cameraController->GetFirstPersonEyeHeight();
    if (ImGui::DragFloat("Eye Height", &eyeHeight, 0.01f, 0.1f, 3.0f, "%.2f"))
    {
        if (playCamera) playCamera->firstPersonEyeOffset.y = eyeHeight;
        m_cameraController->SetFirstPersonEyeHeight(eyeHeight);
    }

    float forwardOffset = playCamera
        ? playCamera->firstPersonEyeOffset.z
        : m_cameraController->GetFirstPersonForwardOffset();
    if (ImGui::DragFloat("Forward Offset", &forwardOffset, 0.01f, -1.0f, 1.0f, "%.2f"))
    {
        if (playCamera) playCamera->firstPersonEyeOffset.z = forwardOffset;
        m_cameraController->SetFirstPersonForwardOffset(forwardOffset);
    }

    float distance = playCamera
        ? playCamera->thirdPersonDistance
        : m_cameraController->GetThirdPersonDistance();
    if (ImGui::DragFloat("Third Distance", &distance, 0.05f, 1.0f, 30.0f, "%.2f"))
    {
        if (playCamera) playCamera->thirdPersonDistance = distance;
        m_cameraController->SetThirdPersonDistance(distance);
    }
    if (playCamera)
    {
        ImGui::DragFloat("Render Distance", &playCamera->renderDistance,
            1.0f, 1.0f, 10000.0f, "%.0f m");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Far clipping distance. Larger values can increase rendering cost.");
    }
    if (playCamera && ImGui::DragFloat3("Third Target Right / Up / Forward",
        &playCamera->thirdPersonTargetOffset.x, 0.01f))
    {
        m_cameraController->SetThirdPersonTargetOffset(
            playCamera->thirdPersonTargetOffset);
        playCamera->thirdPersonTargetOffset =
            m_cameraController->GetThirdPersonTargetOffset();
    }

    float pitch = m_cameraController->GetFollowPitch();
    if (ImGui::DragFloat("Pitch", &pitch, 0.1f, -89.0f, 89.0f, "%.1f"))
        m_cameraController->SetFollowPitch(pitch);

    float lookUpLimit = m_cameraController->GetLookUpLimit();
    if (ImGui::SliderFloat("Third Look Up Limit", &lookUpLimit, 0.0f, 89.0f, "%.1f deg"))
        m_cameraController->SetLookUpLimit(lookUpLimit);

    float lookDownLimit = m_cameraController->GetLookDownLimit();
    if (ImGui::SliderFloat("Third Look Down Limit", &lookDownLimit, 0.0f, 89.0f, "%.1f deg"))
        m_cameraController->SetLookDownLimit(lookDownLimit);

    ImGui::Spacing();
    ImGui::TextColored(COL_TEXT_DIM,
        playCamera ? "Saved on Player. Edit live in Play / Socket Studio."
            : "Add a player camera in Play / Socket Studio to save framing.");
}
//=============================================================================
// RenderSidebar Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ UE5
//=============================================================================
void EditorUI::RenderSidebar(SceneManager& sceneManager)
{
    const ImGuiIO& io = ImGui::GetIO();
    const float    menuBarH = m_menuBarHeight > 0.0f ? m_menuBarHeight : 20.0f;
    const float    height = io.DisplaySize.y - menuBarH;
    const float    sidebarW = SidebarWidth();
    const float    posX = io.DisplaySize.x - sidebarW;

    ImGui::SetNextWindowPos(ImVec2(posX, menuBarH), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(sidebarW, height), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.96f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##Sidebar", nullptr, flags);
    ImGui::PopStyleVar();

    const float headerH = std::floor(44.0f * ScaleY());

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.07f, 0.07f, 1.0f));
    ImGui::BeginChild("##SidebarHeader", ImVec2(-1, headerH), false);
    ImGui::SetCursorPos(ImVec2(10, std::floor(9.0f * ScaleY())));
    ImGui::TextColored(COL_ACCENT, "DX");
    ImGui::SameLine();
    ImGui::Text("Scene Editor");
    ImGui::SameLine(sidebarW - std::floor(70.0f * ScaleX()));
    ImGui::TextColored(COL_TEXT_DIM, "%zu obj", sceneManager.GetObjectCount());
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    const ImGuiTabBarFlags tabFlags =
        ImGuiTabBarFlags_FittingPolicyResizeDown |
        ImGuiTabBarFlags_NoTooltip;

    auto* clipShader = ShaderManager::Get().GetAs<ClipPlaneShaderClass>("clip");
    RenderKnifeTool(sceneManager, clipShader);

    if (ImGui::BeginTabBar("##SidebarTabs", tabFlags))
    {
        if (ImGui::BeginTabItem("Outliner"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
            RenderSceneOutliner(sceneManager);
            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Debug"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

            // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ ApplicationClass (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦. Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦)
            extern PhysicsDebugSettings g_physicsDebugSettings;  // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
            RenderPhysicsDebugPanel();

            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Camera"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
            RenderCameraPanel(sceneManager);
            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }
        /* if (ImGui::BeginTabItem("Details"))
         {
             ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
             RenderDetailsPanel(sceneManager);
             ImGui::PopStyleVar();
             ImGui::EndTabItem();
         }*/
        if (ImGui::BeginTabItem("Spawn"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
            RenderSpawnPanel(sceneManager);
            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("World"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
            RenderFogPanel(sceneManager);

            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Content"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
            RenderContentBrowser();
            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Joint "))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
            RenderJointConnectionUI(sceneManager.GetECSRegistry(), m_selectedEntity);
            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }

        


        RenderShaderSelector(sceneManager);

        ImGui::EndTabBar();
    }
    ImGui::PopStyleVar();

    ImGui::End();

    const float overlayOffsetX = std::floor(10.0f * ScaleX());
    const float overlayOffsetY = menuBarH + std::floor(6.0f * ScaleY());
    ImGui::SetNextWindowPos(ImVec2(overlayOffsetX, overlayOffsetY), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.70f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    if (ImGui::Begin("##Selection", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        switch (m_selectedType)
        {
        case SelectedType::Object:
            ImGui::TextColored(COL_ACCENT, "Selected"); ImGui::SameLine();
            ImGui::TextUnformatted(
                EditorEntityDebugLabel(sceneManager.GetECSRegistry(), m_selectedEntity).c_str());
            break;
        case SelectedType::Light:
            ImGui::TextColored(ImVec4(1, 0.9f, 0.3f, 1), "Light"); ImGui::SameLine();
            ImGui::TextUnformatted(
                EditorEntityDebugLabel(sceneManager.GetECSRegistry(), m_selectedEntity).c_str());
            break;
        case SelectedType::Sprite:
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 1, 1), "Sprite"); ImGui::SameLine();
            ImGui::Text("Sprite #%d", m_selectedLegacyIndex);
            break;
        default:
            ImGui::TextColored(COL_TEXT_DIM, "Nothing selected");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

//=============================================================================
// SetScenePreviewTexture
//=============================================================================
void EditorUI::SetScenePreviewTexture(ID3D11ShaderResourceView* srv)
{
    m_scenePreviewSRV = srv;
}

//=============================================================================
// RenderScenePreview Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ render-to-texture Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
//=============================================================================
void EditorUI::RenderScenePreview()
{
    if (!m_scenePreviewSRV) return;

    const float previewW = std::floor(BASE_WIDTH * PREVIEW_WIDTH_RATIO * ScaleX());
    const float previewH = std::floor(BASE_HEIGHT * PREVIEW_HEIGHT_RATIO * ScaleY());
    const float menuBarH = m_menuBarHeight > 0.0f ? m_menuBarHeight : 20.0f;
    const float sidebarW = SidebarWidth();
    const float posX = ImGui::GetIO().DisplaySize.x - sidebarW - previewW - std::floor(8.0f * ScaleX());
    const float posY = menuBarH + std::floor(8.0f * ScaleY());

    ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(previewW, previewH + std::floor(22.0f * ScaleY())), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    if (ImGui::Begin("##ScenePreview", nullptr, flags))
    {
        ImGui::TextColored(COL_TEXT_DIM, "Scene Preview");
        ImGui::Image((ImTextureID)m_scenePreviewSRV, ImVec2(previewW - 8.0f, previewH));
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

//=============================================================================
// RenderSpawnPanel Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
//=============================================================================
void EditorUI::RenderSpawnPanel(SceneManager& sceneManager)
{
    ImGui::Spacing();
    SectionHeader("Spawn Object");
    ImGui::Spacing();

    ImGui::TextColored(COL_TEXT_DIM, " Model path");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##SpawnModel", m_spawnModelPath, sizeof(m_spawnModelPath));

    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ GLB Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    const std::string modelExt = fs::path(m_spawnModelPath).extension().string();
    const bool isGLB = (modelExt == ".glb" || modelExt == ".gltf");

    ImGui::Spacing();

    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ GLB Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    if (isGLB)
    {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f),
            " GLB: async static mesh; ship import keeps embedded materials");
    }
    else
    {
        ImGui::TextColored(COL_TEXT_DIM, " Texture path");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##SpawnTex", m_spawnTexturePath, sizeof(m_spawnTexturePath));
    }

    ImGui::Spacing();
    ImGui::TextColored(COL_TEXT_DIM, " Position");
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat3("##SpawnPos", m_spawnPos, 0.1f);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(COL_TEXT_DIM, " Model presets");
    for (int i = 0; i < PRESET_COUNT; ++i)
    {
        if (i > 0) ImGui::SameLine(0, 4);
        if (ImGui::SmallButton(MODEL_PRESETS[i]))
            strncpy_s(m_spawnModelPath, sizeof(m_spawnModelPath), MODEL_PRESETS[i], _TRUNCATE);
    }

    ImGui::Spacing();

    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦-GLB Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    if (!isGLB)
    {
        ImGui::TextColored(COL_TEXT_DIM, " Texture presets");
        for (int i = 0; i < PRESET_COUNT; ++i)
        {
            if (i > 0) ImGui::SameLine(0, 4);
            if (ImGui::SmallButton(TEXTURE_PRESETS[i]))
                strncpy_s(m_spawnTexturePath, sizeof(m_spawnTexturePath), TEXTURE_PRESETS[i], _TRUNCATE);
        }
        ImGui::Spacing();
    }

    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, COL_GREEN);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_GREEN_HOV);

    ImGui::Checkbox("Spawn with physics", &useSeparateCollisionMesh);

    if (useSeparateCollisionMesh)
    {
        const std::string collisionPath = BuildCollisionModelPath(m_spawnModelPath);
        strncpy_s(m_spawnCollisionModelPath, sizeof(m_spawnCollisionModelPath), collisionPath.c_str(), _TRUNCATE);

        std::error_code collisionPathError;
        const bool collisionExists = std::filesystem::exists(collisionPath, collisionPathError);

        ImGui::TextColored(COL_TEXT_DIM, "Collision mesh");
        ImGui::TextWrapped("%s", collisionPath.c_str());
        ImGui::TextColored(collisionExists ? COL_GREEN : COL_RED,
            collisionExists ? "Found" : "Missing");
    }

    const AsyncShipImportSnapshot shipLoad = sceneManager.GetAsyncShipImportSnapshot();
    const bool shipButtonDisabled = shipLoad.active || sceneManager.HasImportedShip();
    ImGui::BeginDisabled(shipButtonDisabled);
    if (ImGui::Button(
        shipLoad.active ? "Ship loading..." : "SpawnShip",
        ImVec2(-1, 0)))
    {
        // UI Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦. Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ ApplicationClass,
        // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Direct3D device/context.
        m_importShipRequest.pending = true;
        m_importShipRequest.path = "assets/model/SpaceShip_optimized.glb";
    }
    ImGui::EndDisabled();

    if (ImGui::Button("  Create Collision  ", ImVec2(-1, 0)))
    {
        auto& registry = sceneManager.GetECSRegistry();
        Entity collisionEntity = registry.CreateEntity();

        registry.AddComponent<TransformComponent>(collisionEntity, TransformComponent{
            { m_spawnPos[0], m_spawnPos[1], m_spawnPos[2] },
            { 0.0f, 0.0f, 0.0f },
            { 1.0f, 1.0f, 1.0f }
        });
        registry.AddComponent<NameComponent>(collisionEntity, NameComponent{ "Collision" });
        registry.AddComponent<ActiveComponent>(collisionEntity, {});

        RigidBodyComponent rb{};
        rb.mass = 0.0f;
        rb.isDynamic = false;
        rb.shapeType = RigidBodyComponent::Box;
        rb.shapeSize = btVector3(2.0f, 2.0f, 2.0f);
        rb.useManualSize = true;
        registry.AddComponent<RigidBodyComponent>(collisionEntity, std::move(rb));
        PhysicsSystem::CreateRigidBody(registry, collisionEntity);

        SetSelection(SelectedType::Object, collisionEntity);
    }

    ImGui::Spacing();
    if (ImGui::Button("  Spawn  ", ImVec2(-1, 0)))
    {
        m_spawnRequest.pending = true;
        m_spawnRequest.modelPath = m_spawnModelPath;
        // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ GLB Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
        m_spawnRequest.texturePath = isGLB ? "" : m_spawnTexturePath;
        m_spawnRequest.posX = m_spawnPos[0];
        m_spawnRequest.posY = m_spawnPos[1];
        m_spawnRequest.posZ = m_spawnPos[2];
        m_spawnRequest.useSeparateCollisionMesh = useSeparateCollisionMesh;
        if (useSeparateCollisionMesh)
            m_spawnRequest.collisionModelPath = m_spawnCollisionModelPath;
    }
    ImGui::PopStyleColor(2);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    RenderParticleEditor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    RenderWorldAreaSpawner(sceneManager);
}

void EditorUI::RenderParticleEditor()
{
    SectionHeader("Image Particle Emitter");
    ImGui::TextWrapped(
        "Use a PNG/TGA with alpha. A static picture is a 1x1 atlas; an animated "
        "sprite sheet contains several equally sized frames.");

    ParticleSystem* particles = ParticleSystem::GetActiveSystem();
    if (!particles)
    {
        ImGui::TextColored(COL_RED, "ParticleSystem is not available.");
        return;
    }

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("Texture path##Particle", m_particleTexturePath, sizeof(m_particleTexturePath));
    std::error_code textureError;
    const bool textureExists = std::filesystem::is_regular_file(m_particleTexturePath, textureError);
    ImGui::TextColored(textureExists ? COL_GREEN : COL_RED,
        textureExists ? "Texture found" : "Texture is missing");
    ImGui::BeginDisabled(!textureExists);
    if (ImGui::Button("Reload texture from disk", ImVec2(-1.0f, 0.0f)))
    {
        particles->ReloadTextures();
        m_particleEditorStatus = "Particle texture cache cleared";
    }
    ImGui::EndDisabled();

    ImGui::DragFloat3("Position##Particle", m_particlePosition, 0.05f);
    ImGui::DragFloat3("Direction##Particle", m_particleDirection, 0.02f, -1.0f, 1.0f);
    ImGui::DragFloat3("Acceleration##Particle", m_particleAcceleration, 0.05f);
    ImGui::ColorEdit4("Start color##Particle", m_particleStartColor);
    ImGui::ColorEdit4("End color##Particle", m_particleEndColor);

    if (ImGui::TreeNodeEx("Emission and movement##Particle", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat("Particles / second", &m_particleRate, 0.5f, 0.0f, 1000.0f);
        ImGui::DragFloat("Spawn radius", &m_particleRadius, 0.01f, 0.0f, 100.0f);
        ImGui::SliderFloat("Direction spread", &m_particleSpread, 0.0f, 2.0f);
        ImGui::DragFloat2("Speed min / max", m_particleSpeed, 0.05f, 0.0f, 100.0f);
        ImGui::DragFloat2("Lifetime min / max", m_particleLifetime, 0.05f, 0.01f, 120.0f);
        ImGui::DragFloat2("Start size min / max", m_particleStartSize, 0.01f, 0.001f, 100.0f);
        ImGui::DragFloat2("End size min / max", m_particleEndSize, 0.01f, 0.001f, 100.0f);
        ImGui::DragFloat2("Rotation speed min / max", m_particleRotationSpeed, 0.05f, -50.0f, 50.0f);
        ImGui::DragFloat("Drag", &m_particleDrag, 0.02f, 0.0f, 50.0f);
        ImGui::DragFloat("Aspect ratio", &m_particleAspectRatio, 0.01f, 0.05f, 20.0f);
        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Sprite sheet animation##Particle", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragInt("Columns", &m_particleAtlasColumns, 0.1f, 1, 64);
        ImGui::DragInt("Rows", &m_particleAtlasRows, 0.1f, 1, 64);
        const int maxFrames = (std::max)(1, m_particleAtlasColumns * m_particleAtlasRows);
        m_particleAtlasFrameCount = (std::clamp)(m_particleAtlasFrameCount, 1, maxFrames);
        ImGui::DragInt("Frames used", &m_particleAtlasFrameCount, 0.1f, 1, maxFrames);
        ImGui::DragFloat("Frames / second", &m_particleAtlasFps, 0.1f, 0.0f, 120.0f);
        ImGui::Checkbox("Loop animation", &m_particleAtlasLoop);
        ImGui::Checkbox("Random start frame", &m_particleRandomStartFrame);
        ImGui::TextColored(COL_TEXT_DIM,
            "FPS = 0 distributes all frames across the particle lifetime.");
        ImGui::TreePop();
    }

    const char* blendModes[] = { "Alpha (smoke/dust)", "Additive (fire/glow)" };
    ImGui::Combo("Blend mode", &m_particleBlendMode, blendModes, IM_ARRAYSIZE(blendModes));

    const auto makeDescription = [&]()
    {
        ParticleEmitterDesc desc;
        desc.position = { m_particlePosition[0], m_particlePosition[1], m_particlePosition[2] };
        desc.direction = { m_particleDirection[0], m_particleDirection[1], m_particleDirection[2] };
        desc.acceleration = { m_particleAcceleration[0], m_particleAcceleration[1], m_particleAcceleration[2] };
        desc.startColor = { m_particleStartColor[0], m_particleStartColor[1], m_particleStartColor[2], m_particleStartColor[3] };
        desc.endColor = { m_particleEndColor[0], m_particleEndColor[1], m_particleEndColor[2], m_particleEndColor[3] };
        desc.particlesPerSecond = (std::max)(0.0f, m_particleRate);
        desc.positionRadius = (std::max)(0.0f, m_particleRadius);
        desc.directionSpread = (std::max)(0.0f, m_particleSpread);
        desc.speedMin = (std::min)(m_particleSpeed[0], m_particleSpeed[1]);
        desc.speedMax = (std::max)(m_particleSpeed[0], m_particleSpeed[1]);
        desc.lifetimeMin = (std::min)(m_particleLifetime[0], m_particleLifetime[1]);
        desc.lifetimeMax = (std::max)(m_particleLifetime[0], m_particleLifetime[1]);
        desc.startSizeMin = (std::min)(m_particleStartSize[0], m_particleStartSize[1]);
        desc.startSizeMax = (std::max)(m_particleStartSize[0], m_particleStartSize[1]);
        desc.endSizeMin = (std::min)(m_particleEndSize[0], m_particleEndSize[1]);
        desc.endSizeMax = (std::max)(m_particleEndSize[0], m_particleEndSize[1]);
        desc.rotationSpeedMin = (std::min)(m_particleRotationSpeed[0], m_particleRotationSpeed[1]);
        desc.rotationSpeedMax = (std::max)(m_particleRotationSpeed[0], m_particleRotationSpeed[1]);
        desc.drag = (std::max)(0.0f, m_particleDrag);
        desc.aspectRatio = (std::max)(0.05f, m_particleAspectRatio);
        desc.shape = ParticleShape::Texture;
        desc.blendMode = m_particleBlendMode == 0
            ? ParticleBlendMode::Alpha
            : ParticleBlendMode::Additive;
        desc.texturePath = m_particleTexturePath;
        desc.atlasColumns = static_cast<uint16_t>((std::clamp)(m_particleAtlasColumns, 1, 64));
        desc.atlasRows = static_cast<uint16_t>((std::clamp)(m_particleAtlasRows, 1, 64));
        desc.atlasFrameCount = static_cast<uint16_t>((std::clamp)(
            m_particleAtlasFrameCount,
            1,
            m_particleAtlasColumns * m_particleAtlasRows));
        desc.atlasFramesPerSecond = (std::max)(0.0f, m_particleAtlasFps);
        desc.atlasLoop = m_particleAtlasLoop;
        desc.randomStartFrame = m_particleRandomStartFrame;
        return desc;
    };

    ImGui::BeginDisabled(!textureExists);
    if (ImGui::Button(
        m_particlePreviewEmitter == INVALID_PARTICLE_EMITTER
            ? "Create live emitter"
            : "Restart live emitter",
        ImVec2(-1.0f, 0.0f)))
    {
        if (m_particlePreviewEmitter != INVALID_PARTICLE_EMITTER)
            particles->DestroyEmitter(m_particlePreviewEmitter);
        m_particlePreviewEmitter = particles->CreateEmitter(makeDescription());
        m_particleEditorStatus = m_particlePreviewEmitter == INVALID_PARTICLE_EMITTER
            ? "Emitter limit reached"
            : "Live emitter created";
    }

    ImGui::BeginDisabled(m_particlePreviewEmitter == INVALID_PARTICLE_EMITTER);
    if (ImGui::Button("Apply to live emitter", ImVec2(-1.0f, 0.0f)))
    {
        const bool updated = particles->UpdateEmitter(m_particlePreviewEmitter, makeDescription());
        m_particleEditorStatus = updated ? "Emitter updated without restart" : "Emitter no longer exists";
        if (!updated)
            m_particlePreviewEmitter = INVALID_PARTICLE_EMITTER;
    }
    ImGui::EndDisabled();

    ImGui::DragInt("Burst count", &m_particleBurstCount, 1.0f, 1, 1024);
    if (ImGui::Button("Emit one burst", ImVec2(-1.0f, 0.0f)))
    {
        particles->EmitBurst(makeDescription(), (std::clamp)(m_particleBurstCount, 1, 1024));
        m_particleEditorStatus = "Burst emitted";
    }
    ImGui::EndDisabled();

    ImGui::BeginDisabled(m_particlePreviewEmitter == INVALID_PARTICLE_EMITTER);
    if (ImGui::Button("Delete live emitter", ImVec2(-1.0f, 0.0f)))
    {
        particles->DestroyEmitter(m_particlePreviewEmitter);
        m_particlePreviewEmitter = INVALID_PARTICLE_EMITTER;
        m_particleEditorStatus = "Live emitter deleted; existing particles finish naturally";
    }
    ImGui::EndDisabled();

    ImGui::Text("Alive: %zu / %zu | Emitters: %zu / %zu",
        particles->GetAliveParticleCount(), ParticleSystem::MAX_PARTICLES,
        particles->GetEmitterCount(), ParticleSystem::MAX_EMITTERS);
    if (!m_particleEditorStatus.empty())
        ImGui::TextColored(COL_TEXT_DIM, "%s", m_particleEditorStatus.c_str());
}

void EditorUI::RenderFogPanel(SceneManager& sceneManager)
{
    auto& world = sceneManager.GetWorldSettings();

    ImGui::Text("Skybox HDR");
    ImGui::SetNextItemWidth(-1.0f);

    ImGui::InputText("##SkyboxPath", m_skyboxPathBuf, sizeof(m_skyboxPathBuf));

    if (ImGui::Button("Apply Skybox"))
    {
        world.skyboxPath = m_skyboxPathBuf;
        world.skyboxReloadPending = true;
    }

    ImGui::SliderFloat("Sky Exposure", &world.skyExposure, 0.1f, 10.0f);

    bool sunChanged = false;
    sunChanged |= ImGui::SliderFloat("Sun Dir X", &world.sunDirection.x, -1.0f, 1.0f);
    sunChanged |= ImGui::SliderFloat("Sun Dir Y", &world.sunDirection.y, -1.0f, 1.0f);
    sunChanged |= ImGui::SliderFloat("Sun Dir Z", &world.sunDirection.z, -1.0f, 1.0f);

    (void)sunChanged;

    ImGui::SliderFloat("Sun Intensity", &world.sunIntensity, 0.0f, 20.0f);

    ImGui::SliderFloat("Ambient Strength", &world.ambientStrength, 0.0f, 2.0f);

    ImGui::Checkbox("Enabled Fog", &world.fogEnabled);

    ImGui::BeginDisabled(!world.fogEnabled);

    float col[3] = { world.fogColor.x, world.fogColor.y, world.fogColor.z };
    if (ImGui::ColorEdit3("Fog Color", col))
    {
        world.fogColor = { col[0], col[1], col[2] };
    }

    if (ImGui::DragFloat("Fog Start", &world.fogStart, 0.5f, 0.0f, 1000.0f))
    {
        world.fogEnd = (std::max)(world.fogEnd, world.fogStart + 0.1f);
    }

    if (ImGui::DragFloat("Fog End", &world.fogEnd, 0.5f, 0.0f, 1000.0f))
    {
        world.fogEnd = (std::max)(world.fogEnd, world.fogStart + 0.1f);
    }

    ImGui::EndDisabled();
}
//=============================================================================
// RenderSpriteProperties
//=============================================================================
void EditorUI::RenderSpriteProperties(WorldSpriteClass* sprite)
{
    if (!sprite) return;

    SectionHeader("Sprite");
    ImGui::Spacing();

    XMFLOAT3 sprPos = sprite->GetPosition();
    float pos[3] = { sprPos.x, sprPos.y, sprPos.z };
    ImGui::TextColored(COL_TEXT_DIM, " Position");
    if (DragFloat3Colored("##SprPos", pos, 0.1f))
        sprite->SetPosition(pos[0], pos[1], pos[2]);

    ImGui::Spacing();

    XMFLOAT2 sprScale = sprite->GetScale();
    float w = sprScale.x, h = sprScale.y;
    ImGui::TextColored(COL_TEXT_DIM, " Size");
    ImGui::SetNextItemWidth((ImGui::GetContentRegionAvail().x - 4.0f) * 0.5f);
    ImGui::DragFloat("##SprW", &w, 0.01f, 0.001f, 100.0f);
    ImGui::SameLine(0, 4);
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##SprH", &h, 0.01f, 0.001f, 100.0f);
    sprite->SetScale(w, h);
}

//=============================================================================
// RenderLightProperties
//=============================================================================
void EditorUI::RenderLightPropertiesByEntity(SceneManager& sceneManager, Entity lightEntity)
{
    auto& registry = sceneManager.GetECSRegistry();

    auto* light = registry.GetComponent<LightComponent>(lightEntity);
    auto* transform = registry.GetComponent<TransformComponent>(lightEntity);

    if (!light || !transform)
    {
        ImGui::Text("Invalid light entity");
        return;
    }

    ImGui::Text("Light Properties");

    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    float color[4] = { light->color.x, light->color.y, light->color.z, light->color.w };
    if (ImGui::ColorEdit4("Color", color))
    {
        light->color = { color[0], color[1], color[2], color[3] };
    }

    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦)
    if (light->attachedTo == INVALID_ENTITY)
    {
        float pos[3] = { transform->position.x, transform->position.y, transform->position.z };
        if (ImGui::DragFloat3("Position", pos, 0.1f))
        {
            transform->position = { pos[0], pos[1], pos[2] };
            // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦, Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
        }
    }

    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    ImGui::Checkbox("Enabled", &light->enabled);

    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    if (ImGui::Button("Delete Light"))
        RequestDeleteSelection();
}

//=============================================================================
// RenderContentBrowser
//=============================================================================
void EditorUI::RenderContentBrowser()
{
    ImGui::Spacing();
    RenderFileBrowserToolbar();
    ImGui::Spacing();
    RenderFileBrowserBreadcrumb();
    ImGui::Spacing();

    if (!m_fileSystem) { ImGui::TextColored(COL_TEXT_DIM, "FileSystem not initialized"); return; }

    const std::string query = m_searchQuery;

    // ? FIX: Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    std::string relativeDir = m_currentDir.string();
    if (relativeDir == ".") {
        relativeDir = "";
    }

    auto entries = m_fileSystem->GetFilteredEntries(relativeDir, query, m_filterExtensions[m_activeFilterIndex]);

    // ? DEBUG: Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦


    RenderFileBrowserList(entries);
}

void EditorUI::RenderFileBrowserToolbar()
{
    const float btnW = std::floor(28.0f * ScaleX());

    // ? FIX: Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦, Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    bool canGoBack = !m_currentDir.empty() && m_currentDir != ".";

    ImGui::BeginDisabled(!canGoBack);
    if (ImGui::Button("<##Back", ImVec2(btnW, 0)) && m_fileSystem)
    {
        fs::path currentPath(m_currentDir);
        fs::path parentPath = currentPath.parent_path();

        // ? FIX: Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ parent Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦)
        m_currentDir = parentPath.empty() ? "" : parentPath.generic_string();

        // ? DEBUG: Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
        OutputDebugStringA(("[NAV] Back to: '" + m_currentDir.string() + "'\n").c_str());

        // ? FIX: Refresh() Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦! Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
        // m_fileSystem->Refresh(); 
    }
    if (ImGui::IsItemHovered() && !canGoBack) {
        ImGui::SetTooltip("Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦");
    }
    ImGui::EndDisabled();

    ImGui::SameLine(0, 4);
    if (ImGui::Button("R##Refresh", ImVec2(btnW, 0)) && m_fileSystem)
        m_fileSystem->Refresh();

    ImGui::SameLine(0, 8);
    ImGui::SetNextItemWidth(std::floor(90.0f * ScaleX()));
    if (ImGui::BeginCombo("##Filter", m_filterLabels[m_activeFilterIndex].c_str()))
    {
        for (int i = 0; i < (int)m_filterLabels.size(); ++i)
            if (ImGui::Selectable(m_filterLabels[i].c_str(), m_activeFilterIndex == i))
                m_activeFilterIndex = i;
        ImGui::EndCombo();
    }

    ImGui::SameLine(0, 8);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##Search", m_searchBuf, sizeof(m_searchBuf));
    m_searchQuery = m_searchBuf;

    ImGui::SameLine(0, 8);
    ImGui::PushStyleColor(ImGuiCol_Button, m_viewModeGrid ? COL_ACCENT : ImVec4(0.22f, 0.22f, 0.22f, 1));
    if (ImGui::Button("##Grid", ImVec2(btnW, 0))) m_viewModeGrid = true;
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 2);
    ImGui::PushStyleColor(ImGuiCol_Button, !m_viewModeGrid ? COL_ACCENT : ImVec4(0.22f, 0.22f, 0.22f, 1));
    if (ImGui::Button("##List", ImVec2(btnW, 0))) m_viewModeGrid = false;
    ImGui::PopStyleColor();
}

void EditorUI::RenderFileBrowserBreadcrumb()
{
    ImGui::TextColored(COL_TEXT_DIM, "/");
    ImGui::TextColored(COL_TEXT_DIM, "/");
    ImGui::SameLine(0, 2);
    std::string dirStr = m_currentDir.string();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "%s", dirStr.empty() ? "root" : dirStr.c_str());
}

void EditorUI::RenderFileBrowserList(const std::vector<const FileEntry*>& entries)
{
    const float avail = ImGui::GetContentRegionAvail().y - std::floor(4.0f * ScaleY());
    ImGui::BeginChild("##CBList", ImVec2(-1, avail), false);

    if (m_viewModeGrid)
    {
        const float cellW = std::floor(70.0f * ScaleX());
        int cols = (std::max)(1, (int)(ImGui::GetContentRegionAvail().x / cellW));
        if (ImGui::BeginTable("##CBGrid", cols))
        {
            for (size_t i = 0; i < entries.size(); ++i)
            {
                ImGui::TableNextColumn();
                RenderFileBrowserItemGrid(entries[i], i);
            }
            ImGui::EndTable();
        }
    }
    else
    {
        for (size_t i = 0; i < entries.size(); ++i)
            RenderFileBrowserItemList(entries[i], i);
    }

    ImGui::EndChild();
}

void EditorUI::RenderFileBrowserItemGrid(const FileEntry* entry, size_t index)
{
    if (!entry) return;
    ImGui::PushID((int)index);
    const bool sel = (m_selectedEntry == entry);
    if (sel) ImGui::PushStyleColor(ImGuiCol_Button, COL_ACCENT);
    if (ImGui::Button(entry->isDirectory ? "[D]" : "[F]", ImVec2(std::floor(56.0f * ScaleX()), std::floor(48.0f * ScaleY()))))
        OnEntrySelected(entry);
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
        OnEntryDoubleClicked(entry);
    if (sel) ImGui::PopStyleColor();
    ImGui::TextColored(COL_TEXT_DIM, "%s", entry->name.c_str());
    ImGui::PopID();
}

void EditorUI::RenderFileBrowserItemList(const FileEntry* entry, size_t index)
{
    if (!entry) return;
    ImGui::PushID((int)index);
    const bool sel = (m_selectedEntry == entry);
    if (sel) ImGui::PushStyleColor(ImGuiCol_Header, COL_SELECTED);
    char label[256];
    snprintf(label, sizeof(label), "%s  %s", entry->isDirectory ? "[D]" : "[F]", entry->name.c_str());
    if (ImGui::Selectable(label, sel, 0, ImVec2(0, std::floor(18.0f * ScaleY()))))
        OnEntrySelected(entry);
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
        OnEntryDoubleClicked(entry);
    if (sel) ImGui::PopStyleColor();
    ImGui::PopID();
}

void EditorUI::OnEntryDoubleClicked(const FileEntry* entry)
{
    if (!entry) return;
    if (entry->isDirectory)
    {
        m_currentDir = fs::path(m_currentDir) / entry->name;
        if (m_fileSystem) m_fileSystem->Refresh();
    }
    else
    {
        OnFileSelected(entry);
    }
}

void EditorUI::OnEntrySelected(const FileEntry* entry)
{
    m_selectedEntry = entry;
    if (entry && !entry->isDirectory)
        OnFileSelected(entry);
}

void EditorUI::OnFileSelected(const FileEntry* entry)
{
    if (!entry) return;
    std::string ext = fs::path(entry->name).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Model path
    if (ext == ".obj" || ext == ".fbx" || ext == ".glb" || ext == ".gltf")
        strncpy_s(m_spawnModelPath, sizeof(m_spawnModelPath), entry->path.c_str(), _TRUNCATE);
    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Texture path
    else if (ext == ".tga" || ext == ".png" || ext == ".jpg" || ext == ".dds")
        strncpy_s(m_spawnTexturePath, sizeof(m_spawnTexturePath), entry->path.c_str(), _TRUNCATE);
}
//=============================================================================
// RenderKnifeTool Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (clip plane)
//=============================================================================
void EditorUI::RenderKnifeTool(SceneManager& sceneManager, ClipPlaneShaderClass* clipShader)
{
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 1.0f));
    ImGui::BeginChild("##KnifeToolBar", ImVec2(-1, std::floor(32.0f * ScaleY())), false);

    ImGui::PushStyleColor(ImGuiCol_Button, m_knifeEnabled ? COL_ACCENT : ImVec4(0.22f, 0.22f, 0.22f, 1));
    if (ImGui::Button("  Knife  ", ImVec2(0, 0)))
        m_knifeEnabled = !m_knifeEnabled;
    ImGui::PopStyleColor();

    if (m_knifeEnabled && clipShader)
    {
        ImGui::SameLine(0, 8);
        ImGui::SetNextItemWidth(std::floor(60.0f * ScaleX()));
        const char* axes[] = { "X", "Y", "Z" };
        ImGui::Combo("##KAxis", &m_knifeAxis, axes, 3);

        ImGui::SameLine(0, 8);
        ImGui::SetNextItemWidth(std::floor(80.0f * ScaleX()));
        ImGui::DragFloat("##KPos", &m_knifePosition, 0.1f);

        ImGui::SameLine(0, 8);
        ImGui::Checkbox("Flip", &m_knifeFlip);

        XMFLOAT4 plane = { 0, 0, 0, -m_knifePosition };
        if (m_knifeAxis == 0) plane.x = m_knifeFlip ? -1.0f : 1.0f;
        else if (m_knifeAxis == 1) plane.y = m_knifeFlip ? -1.0f : 1.0f;
        else                       plane.z = m_knifeFlip ? -1.0f : 1.0f;
        clipShader->SetClipPlane(plane);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

//=============================================================================
// RenderShaderSelector Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
//=============================================================================
void EditorUI::RenderShaderSelector(SceneManager& sceneManager)
{
    if (ImGui::BeginTabItem("Shader"))
    {
        ImGui::Spacing();
        SectionHeader("Shader Selector");
        ImGui::Spacing();

        if (m_shaderNames.empty())
            RefreshShaderList();

        if (m_shaderNames.empty())
        {
            ImGui::TextColored(COL_TEXT_DIM, "No shaders registered");
            ImGui::EndTabItem();
            return;
        }

        std::vector<const char*> names;
        names.reserve(m_shaderNames.size());
        for (auto& n : m_shaderNames) names.push_back(n.c_str());

        ImGui::SetNextItemWidth(-1);
        ImGui::ListBox("##ShaderList", &m_selectedShaderIndex, names.data(), (int)names.size(), 6);

        ImGui::Spacing();

        const bool hasObject = (m_selectedType == SelectedType::Object &&
            m_selectedEntity != INVALID_ENTITY);

        ImGui::BeginDisabled(!hasObject);
        ImGui::PushStyleColor(ImGuiCol_Button, COL_ACCENT);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_ACCENT_HOV);
        if (ImGui::Button("Apply Shader", ImVec2(-1, 0)) && hasObject)
        {
            auto* shader = ShaderManager::Get().Get(m_shaderNames[m_selectedShaderIndex]);
            if (shader) sceneManager.SetObjectShader(m_selectedEntity, shader, m_device);
        }
        ImGui::PopStyleColor(2);
        ImGui::EndDisabled();

        if (!hasObject)
            ImGui::TextColored(COL_TEXT_DIM, "Select an object first");

        ImGui::Spacing();
        if (ImGui::Button("Refresh List", ImVec2(-1, 0)))
            RefreshShaderList();

        // -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ ----------------------------------------------------
        if (hasObject)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            SectionHeader("Textures");
            ImGui::Spacing();

            auto* meshForTerrain = sceneManager.GetECSRegistry().GetComponent<MeshRendererComponent>(m_selectedEntity);
            MaterialClass* terrainMat = meshForTerrain ? meshForTerrain->material : nullptr;
            ShaderBaseClass* activeShader = terrainMat ? terrainMat->GetShader() : nullptr;
            const bool isTerrainBlend = activeShader && activeShader->GetType() == ShaderType::TerrainBlend;

            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.10f, 0.24f, 0.14f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.15f, 0.34f, 0.20f, 1.0f));
            bool terrainOpen = ImGui::CollapsingHeader("  Terrain Blend", ImGuiTreeNodeFlags_DefaultOpen);
            ImGui::PopStyleColor(2);

            if (terrainOpen)
            {
                ImGui::Spacing();
                ImGui::TextColored(COL_TEXT_DIM, "GLB terrain slots: 0 grass, 1 dirt, 2 mask, 5 grass variation");

                ImGui::PushStyleColor(ImGuiCol_Button, COL_GREEN);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_GREEN_HOV);
                if (ImGui::Button(isTerrainBlend ? "Terrain Blend Active" : "Apply Terrain Blend Shader", ImVec2(-1, 0)))
                {
                    auto* shader = ShaderManager::Get().Get("terrain_blend");
                    if (shader) sceneManager.SetObjectShader(m_selectedEntity, shader, m_device);
                }
                ImGui::PopStyleColor(2);

                ImGui::Spacing();
                ImGui::TextColored(COL_TEXT_DIM, " Grass Base Color");
                ImGui::SetNextItemWidth(-1);
                ImGui::InputText("##TerrainGrassPath", m_terrainGrassPathBuf, sizeof(m_terrainGrassPathBuf));

                ImGui::TextColored(COL_TEXT_DIM, " Dirt Base Color");
                ImGui::SetNextItemWidth(-1);
                ImGui::InputText("##TerrainDirtPath", m_terrainDirtPathBuf, sizeof(m_terrainDirtPathBuf));

                ImGui::TextColored(COL_TEXT_DIM, " Dirt Mask (linear)");
                ImGui::SetNextItemWidth(-1);
                ImGui::InputText("##TerrainMaskPath", m_terrainMaskPathBuf, sizeof(m_terrainMaskPathBuf));

                ImGui::TextColored(COL_TEXT_DIM, " Grass Normal (linear)");
                ImGui::SetNextItemWidth(-1);
                ImGui::InputText("##TerrainGrassNormalPath", m_terrainGrassNormalPathBuf, sizeof(m_terrainGrassNormalPathBuf));

                ImGui::TextColored(COL_TEXT_DIM, " Dirt Normal (linear)");
                ImGui::SetNextItemWidth(-1);
                ImGui::InputText("##TerrainDirtNormalPath", m_terrainDirtNormalPathBuf, sizeof(m_terrainDirtNormalPathBuf));

                ImGui::TextColored(COL_TEXT_DIM, " Grass Variation Base Color");
                ImGui::SetNextItemWidth(-1);
                ImGui::InputText("##TerrainGrassVariationPath", m_terrainGrassVariationPathBuf, sizeof(m_terrainGrassVariationPathBuf));

                ImGui::TextColored(COL_TEXT_DIM, " Grass Variation Normal (linear)");
                ImGui::SetNextItemWidth(-1);
                ImGui::InputText("##TerrainGrassVariationNormalPath", m_terrainGrassVariationNormalPathBuf, sizeof(m_terrainGrassVariationNormalPathBuf));

                ImGui::Spacing();
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("Tile Scale", &m_terrainTileScale, 0.05f, 0.001f, 100.0f, "%.3f");

                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("Normal Strength", &m_terrainNormalStrength, 0.05f, 0.0f, 8.0f, "%.3f");

                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("Variation Scale", &m_terrainVariationScale, 0.005f, 0.001f, 5.0f, "%.3f");

                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("Variation Strength", &m_terrainVariationStrength, 0.05f, 0.0f, 8.0f, "%.3f");

                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("Terrain Brightness", &m_terrainBrightness, 0.02f, 0.0f, 4.0f, "%.3f");

                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("Terrain Ambient Boost", &m_terrainAmbientBoost, 0.02f, 0.0f, 4.0f, "%.3f");

                ImGui::TextColored(COL_TEXT_DIM, " Mask World Min XZ");
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat2("##TerrainMinXZ", m_terrainMinXZ, 0.1f);

                ImGui::TextColored(COL_TEXT_DIM, " Mask World Size XZ");
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat2("##TerrainSizeXZ", m_terrainSizeXZ, 0.1f, 0.001f, 100000.0f);

                ImGui::PushStyleColor(ImGuiCol_Button, COL_ACCENT);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_ACCENT_HOV);
                if (ImGui::Button("Apply Terrain Textures", ImVec2(-1, 0)))
                {
                    auto* shader = ShaderManager::Get().Get("terrain_blend");
                    if (shader) sceneManager.SetObjectShader(m_selectedEntity, shader, m_device);

                    sceneManager.SetObjectTexture(m_selectedEntity, 0, m_terrainGrassPathBuf, m_device, m_context, true);
                    sceneManager.SetObjectTexture(m_selectedEntity, 1, m_terrainDirtPathBuf, m_device, m_context, true);
                    sceneManager.SetObjectTexture(m_selectedEntity, 2, m_terrainMaskPathBuf, m_device, m_context, false);
                    sceneManager.SetObjectTexture(m_selectedEntity, 3, m_terrainGrassNormalPathBuf, m_device, m_context, false);
                    sceneManager.SetObjectTexture(m_selectedEntity, 4, m_terrainDirtNormalPathBuf, m_device, m_context, false);

                    sceneManager.SetObjectTexture(m_selectedEntity, 5, m_terrainGrassVariationPathBuf, m_device, m_context, true);
                    sceneManager.SetObjectTexture(m_selectedEntity, 6, m_terrainGrassVariationNormalPathBuf, m_device, m_context, false);

                    if (auto* mesh = sceneManager.GetECSRegistry().GetComponent<MeshRendererComponent>(m_selectedEntity))
                    {
                        if (mesh->material)
                        {
                            mesh->material->SetTerrainTileScale(m_terrainTileScale);
                            mesh->material->SetTerrainNormalStrength(m_terrainNormalStrength);
                            mesh->material->SetTerrainVariationScale(m_terrainVariationScale);
                            mesh->material->SetTerrainVariationStrength(m_terrainVariationStrength);
                            mesh->material->SetTerrainBrightness(m_terrainBrightness);
                            mesh->material->SetTerrainAmbientBoost(m_terrainAmbientBoost);
                            mesh->material->SetTerrainBounds(
                                DirectX::XMFLOAT2(m_terrainMinXZ[0], m_terrainMinXZ[1]),
                                DirectX::XMFLOAT2(m_terrainSizeXZ[0], m_terrainSizeXZ[1]));
                        }
                    }
                }
                ImGui::PopStyleColor(2);
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
            }
            // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (embedded) Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ GLB
            auto* mesh = sceneManager.GetECSRegistry().GetComponent<MeshRendererComponent>(m_selectedEntity);
            const bool hasEmbedded = mesh && mesh->model && mesh->model->IsEmbeddedFormat();

            if (hasEmbedded)
            {
                // -- GLB: Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ --
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
                ImGui::TextWrapped("  GLB embedded textures detected.");
                ImGui::PopStyleColor();
                ImGui::TextColored(COL_TEXT_DIM, "  No paths needed Р С—РЎвЂ”Р вЂ¦ textures are");
                ImGui::TextColored(COL_TEXT_DIM, "  loaded automatically from the file.");
                ImGui::Spacing();

                // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                const auto& parts = mesh->model->GetMeshParts();
                ImGui::TextColored(COL_TEXT_DIM, "  Meshes: %d", (int)parts.size());
                for (int pi = 0; pi < (int)parts.size(); ++pi)
                {
                    const auto& p = parts[pi];
                    ImGui::Text("  [%d]  Albedo:%s  Normal:%s  Metal:%s  Rough:%s",
                        pi,
                        p.texture ? "OK" : "--",
                        p.normalMap ? "OK" : "--",
                        p.metalness ? "OK" : "--",
                        p.roughness ? "OK" : "--");
                }

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f),
                    "  Tip: use 'pbr' shader to see all maps.");
            }
            else
            {
                // -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (OBJ/FBX) Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ --------
                const bool isPBR = (m_selectedShaderIndex >= 0 &&
                    m_selectedShaderIndex < (int)m_shaderNames.size() &&
                    m_shaderNames[m_selectedShaderIndex] == "pbr");

                // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ 0
                ImGui::TextColored(COL_TEXT_DIM, isPBR ? " Albedo (slot 0)" : " Texture (slot 0)");
                ImGui::SetNextItemWidth(200.0f); // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                ImGui::InputText("##Tex0Path", m_tex0PathBuf, sizeof(m_tex0PathBuf));
                ImGui::SameLine(0, 4);
                ImGui::PushStyleColor(ImGuiCol_Button, COL_ACCENT);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_ACCENT_HOV);
                if (ImGui::Button("Apply##T0", ImVec2(-1, 0)))
                    sceneManager.SetObjectTexture(m_selectedEntity, 0, m_tex0PathBuf, m_device, m_context);
                ImGui::PopStyleColor(2);

                ImGui::Spacing();

                // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ 1
                ImGui::TextColored(COL_TEXT_DIM, " Normal Map (slot 1)");
                ImGui::SetNextItemWidth(-std::floor(60.0f * ScaleX()) - 4.0f);
                ImGui::InputText("##Tex1Path", m_tex1PathBuf, sizeof(m_tex1PathBuf));
                ImGui::SameLine(0, 4);
                ImGui::PushStyleColor(ImGuiCol_Button, COL_ACCENT);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_ACCENT_HOV);
                if (ImGui::Button("Apply##T1", ImVec2(-1, 0)))
                    sceneManager.SetObjectTexture(m_selectedEntity, 1, m_tex1PathBuf, m_device, m_context);
                ImGui::PopStyleColor(2);

                // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ 2 Р С—РЎвЂ”Р вЂ¦ 3 Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ PBR
                if (isPBR)
                {
                    ImGui::Spacing();

                    ImGui::TextColored(COL_TEXT_DIM, " Metalness (slot 2)");
                    ImGui::SetNextItemWidth(-std::floor(60.0f * ScaleX()) - 4.0f);
                    ImGui::InputText("##Tex2Path", m_tex2PathBuf, sizeof(m_tex2PathBuf));
                    ImGui::SameLine(0, 4);
                    ImGui::PushStyleColor(ImGuiCol_Button, COL_ACCENT);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_ACCENT_HOV);
                    if (ImGui::Button("Apply##T2", ImVec2(-1, 0)))
                        sceneManager.SetObjectTexture(m_selectedEntity, 2, m_tex2PathBuf, m_device, m_context);
                    ImGui::PopStyleColor(2);

                    ImGui::Spacing();

                    ImGui::TextColored(COL_TEXT_DIM, " Roughness (slot 3)");
                    ImGui::SetNextItemWidth(-std::floor(60.0f * ScaleX()) - 4.0f);
                    ImGui::InputText("##Tex3Path", m_tex3PathBuf, sizeof(m_tex3PathBuf));
                    ImGui::SameLine(0, 4);
                    ImGui::PushStyleColor(ImGuiCol_Button, COL_ACCENT);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_ACCENT_HOV);
                    if (ImGui::Button("Apply##T3", ImVec2(-1, 0)))
                        sceneManager.SetObjectTexture(m_selectedEntity, 3, m_tex3PathBuf, m_device, m_context);
                    ImGui::PopStyleColor(2);

                    // ======================================================
                    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                    // ======================================================
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    SectionHeader("Glass / Transparency");
                    ImGui::Spacing();

                    auto* mesh = sceneManager.GetObjectMesh(m_selectedEntity);
                    MaterialClass* mat = mesh ? mesh->material : nullptr;

                    if (mat)
                    {
                        bool isGlass = mat->IsGlass();
                        if (ImGui::Checkbox("Enable Glass Mode", &isGlass))
                        {
                            float currentOpacity = mat->GetGlassOpacity();
                            mat->SetGlass(isGlass, isGlass ? (currentOpacity < 0.99f ? currentOpacity : 0.15f) : 1.0f);
                        }

                        if (isGlass)
                        {
                            ImGui::Spacing();
                            float opacity = mat->GetGlassOpacity();
                            ImGui::TextColored(COL_TEXT_DIM, " Glass Opacity (0=clear, 1=opaque)");
                            ImGui::SetNextItemWidth(-1);
                            if (ImGui::SliderFloat("##GlassOpacity", &opacity, 0.0f, 0.95f, "%.2f"))
                                mat->SetGlass(true, opacity);

                            ImGui::Spacing();
                            ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f),
                                " Tip: 0.05-0.20 = clean window glass\n"
                                " Tip: 0.3-0.6  = tinted/dirty glass\n"
                                " Roughness tex not needed for glass\n"
                                " (shader forces low roughness auto)");
                        }
                        else
                        {
                            ImGui::TextColored(COL_TEXT_DIM,
                                " Off: opaque PBR material");
                        }
                    }
                }
            }
        }

        ImGui::EndTabItem();
    }
}

void EditorUI::RenderPhysicsSettings(SceneManager& sceneManager)
{
    if (ImGui::BeginTabItem("Physics"))
    {
        if (sceneManager.GetECSRegistry().HasComponent<RigidBodyComponent>(m_selectedEntity))
        {

        }
    }

    ImGui::EndTabItem();

}


void EditorUI::RenderJointConnectionUI(ECSRegistry& registry, Entity selectedEntity)
{
    auto* jointComp = registry.GetComponent<JointConnectionComponent>(selectedEntity);

    if (ImGui::CollapsingHeader("Joint Connection", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (!jointComp)
        {
            // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
            if (ImGui::Button("Add Joint Connection"))
            {
                JointConnectionComponent newComp{};
                registry.AddComponent<JointConnectionComponent>(selectedEntity, std::move(newComp));
                jointComp = registry.GetComponent<JointConnectionComponent>(selectedEntity);
            }
        }
        else
        {
            // -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ ----------------------------------------
            ImGui::Text("Target Entity:");
            ImGui::SameLine();

            // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ "None"
            std::string targetName = "None";
            if (jointComp->targetEntity != INVALID_ENTITY)
            {
                targetName = "Entity " + std::to_string(jointComp->targetEntity);
            }

            if (ImGui::Button(targetName.c_str()))
            {
                // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                ImGui::OpenPopup("SelectTargetPopup");
            }

            // Popup Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
            if (ImGui::BeginPopup("SelectTargetPopup"))
            {
                auto& allTransforms = registry.GetComponents<TransformComponent>();
                auto& allRigidBodies = registry.GetComponents<RigidBodyComponent>();

                for (auto& [entity, transform] : allTransforms)
                {
                    if (entity == selectedEntity) continue; // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦

                    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ RigidBody
                    if (!allRigidBodies.Has(entity)) continue;

                    std::string entityName = "Entity " + std::to_string(entity);
                    if (ImGui::Selectable(entityName.c_str()))
                    {
                        // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                        if (jointComp->joint)
                        {
                            JointConnectionSystem::RemoveJoint(registry, *jointComp);
                        }

                        jointComp->targetEntity = entity;
                        jointComp->isBroken = false;
                    }
                }
                ImGui::EndPopup();
            }

            // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
            ImGui::SameLine();
            if (jointComp->targetEntity != INVALID_ENTITY && ImGui::SmallButton("Clear"))
            {
                if (jointComp->joint)
                {

                    JointConnectionSystem::RemoveJoint(registry, *jointComp);
                }
                jointComp->targetEntity = INVALID_ENTITY;
                jointComp->joint = nullptr;
                jointComp->isBroken = false;
            }

            ImGui::Separator();

            // -- Pivot A (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦) -----------------
            ImGui::Text("Pivot A (This):");
            ImGui::DragFloat3("##PivotA", &jointComp->pivotA[0], 0.1f);

            // -- Pivot B (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦) --------------------------
            ImGui::Text("Pivot B (Target):");
            ImGui::DragFloat3("##PivotB", &jointComp->pivotB[0], 0.1f);

            ImGui::Separator();

            // -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ ----------------------------------------------------
            ImGui::DragFloat("Break Force", &jointComp->breakForce, 1000.0f, 0.0f, 1000000.0f);
            ImGui::Checkbox("Disable Collisions", &jointComp->disableCollisions);
            ImGui::Checkbox("Show Gizmo", &jointComp->showGizmo);

            ImGui::ColorEdit4("Gizmo Color", &jointComp->gizmoColor.x);

            ImGui::Separator();

            // -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ -------------------------------------------------------
            ImGui::Text("Status:");
            ImGui::SameLine();

            if (jointComp->isBroken)
            {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "BROKEN");
            }
            else if (jointComp->joint)
            {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "Connected");

                // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                btScalar impulse = 0.f;
                if (jointComp->joint->getJointFeedback()) // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                {
                    impulse = jointComp->joint->getAppliedImpulse();
                }
                else
                {
                    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦: Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ "Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦", Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                    jointComp->joint->enableFeedback(true);
                }
                char buf[128];
                sprintf_s(buf, "Impulse: %.2f / %.2f", impulse, jointComp->breakForce);
                ImGui::Text(buf);
            }
            else if (jointComp->targetEntity != INVALID_ENTITY)
            {
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "Pending...");
            }
            else
            {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "No target");
            }

            // -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ --------------------------------------------
            ImGui::Separator();

            if (jointComp->joint && ImGui::Button("Remove Joint"))
            {
                JointConnectionSystem::RemoveJoint(registry, *jointComp);
                jointComp->joint = nullptr;
            }

            ImGui::SameLine();

            if (ImGui::Button("Auto-Calculate Pivots"))
            {
                // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ pivot'Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                AutoCalculatePivots(registry, selectedEntity, *jointComp);
            }
        }
    }
}

// Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦-Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
void EditorUI::AutoCalculatePivots(ECSRegistry& registry, Entity entity, JointConnectionComponent& jointComp)
{
    auto* transformA = registry.GetComponent<TransformComponent>(entity);
    auto* transformB = registry.GetComponent<TransformComponent>(jointComp.targetEntity);
    auto* rbA = registry.GetComponent<RigidBodyComponent>(entity);
    auto* rbB = registry.GetComponent<RigidBodyComponent>(jointComp.targetEntity);

    if (!transformA || !transformB || !rbA || !rbB) return;

    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ A Р С—РЎвЂ”Р вЂ¦ B
    btVector3 posA(transformA->position.x, transformA->position.y, transformA->position.z);
    btVector3 posB(transformB->position.x, transformB->position.y, transformB->position.z);
    btVector3 dir = posB - posA;
    dir.normalize();

    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    float absX = fabs(dir.x());
    float absY = fabs(dir.y());
    float absZ = fabs(dir.z());

    if (absX > absY && absX > absZ)
    {
        // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ X
        jointComp.pivotA.setValue(dir.x() > 0 ? -rbA->shapeSize.x() : rbA->shapeSize.x(), 0, 0);
        jointComp.pivotB.setValue(dir.x() > 0 ? rbB->shapeSize.x() : -rbB->shapeSize.x(), 0, 0);
    }
    else if (absZ > absX && absZ > absY)
    {
        // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Z
        jointComp.pivotA.setValue(0, 0, dir.z() > 0 ? -rbA->shapeSize.z() : rbA->shapeSize.z());
        jointComp.pivotB.setValue(0, 0, dir.z() > 0 ? rbB->shapeSize.z() : -rbB->shapeSize.z());
    }
    else
    {
        // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Y
        jointComp.pivotA.setValue(0, dir.y() > 0 ? -rbA->shapeSize.y() : rbA->shapeSize.y(), 0);
        jointComp.pivotB.setValue(0, dir.y() > 0 ? rbB->shapeSize.y() : -rbB->shapeSize.y(), 0);
    }
}






void EditorUI::RenderCompartmentEditor(SceneManager& sceneManager)
{
    if (m_selectedType != SelectedType::Object) return;

    auto& registry = sceneManager.GetECSRegistry();
    auto* comp = registry.GetComponent<CompartmentComponent>(m_selectedEntity);
    if (!comp) return;

    ImGui::Begin("Compartment Editor", nullptr);

    // ==================================================
    // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (AABB)
    // ==================================================
    ImGui::Text("Local Bounds:");

    float min[3] = { comp->localMin.x, comp->localMin.y, comp->localMin.z };
    float max[3] = { comp->localMax.x, comp->localMax.y, comp->localMax.z };

    if (ImGui::DragFloat3("Min", min, 0.1f))
    {
        comp->localMin = { min[0], min[1], min[2] };
        comp->RecalculateVolume();
    }

    if (ImGui::DragFloat3("Max", max, 0.1f))
    {
        comp->localMax = { max[0], max[1], max[2] };
        comp->RecalculateVolume();
    }

    // ==================================================
    // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    // ==================================================
    ImGui::Separator();
    ImGui::Text("Visual Settings:");

    static bool showWireframe = true;
    ImGui::Checkbox("Show Wireframe", &showWireframe);

    static float wireframeAlpha = 0.4f;
    ImGui::SliderFloat("Opacity", &wireframeAlpha, 0.0f, 1.0f);

    // ==================================================
    // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    // ==================================================
    ImGui::Separator();
    ImGui::Text("Info:");
    ImGui::Text("Volume: %.2f m?", comp->volume);
    ImGui::Text("Water: %.1f%%", (comp->waterVolume / comp->volume) * 100.0f);

    // ==================================================
    // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    // ==================================================
    ImGui::Separator();

    if (ImGui::Button("Center on Submarine"))
    {
        auto* subLink = registry.GetComponent<SubmarineComponent>(m_selectedEntity);
        if (subLink && subLink->submarineRoot != INVALID_ENTITY)
        {
            auto* rootTransform = registry.GetComponent<TransformComponent>(subLink->submarineRoot);
            auto* myTransform = registry.GetComponent<TransformComponent>(m_selectedEntity);

            if (rootTransform && myTransform)
            {
                // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
                XMFLOAT3 center = {
                    (comp->localMin.x + comp->localMax.x) * 0.5f,
                    (comp->localMin.y + comp->localMax.y) * 0.5f,
                    (comp->localMin.z + comp->localMax.z) * 0.5f
                };
                comp->localCenter = center;
            }
        }
    }

    if (ImGui::Button("Reset to Default"))
    {
        comp->localMin = { -2, -1.5f, -2 };
        comp->localMax = { 2,  1.5f,  2 };
        comp->RecalculateVolume();
    }

    ImGui::End();
}


void EditorUI::RenderPhysicsDebugPanel()
{
    ImGui::Spacing();
    SectionHeader("Physics Visualization");
    ImGui::Spacing();

    // ==================================================================
    // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    // ==================================================================
    ImGui::TextColored(COL_TEXT_DIM, " Render Modes:");
    ImGui::Checkbox("Show Colliders", &m_debugSettings.showColliders);
    ImGui::Checkbox("Show Contacts", &m_debugSettings.showContacts);
    ImGui::Checkbox("Show AABB", &m_debugSettings.showAABB);
    ImGui::Checkbox("Show Center of Mass", &m_debugSettings.showCenterOfMass);
    ImGui::Checkbox("Show Velocity Vectors", &m_debugSettings.showVelocity);
    ImGui::Checkbox("Show Interaction Rays", &m_debugSettings.showInteractionRays);
    ImGui::Checkbox("Show NPC Sight Rays", &m_debugSettings.showNpcSightRays);
    auto& navigationDebug = NpcNavMeshSystem::GetDebugDrawSettings();
    ImGui::Checkbox("Navigation Debug", &navigationDebug.enabled);
    ImGui::Checkbox("Show NavMesh Grid", &navigationDebug.drawMesh);
    ImGui::Checkbox("Show NPC Routes", &navigationDebug.drawPath);
    ImGui::Checkbox("Show Cover Points", &navigationDebug.drawCoverPoints);
    ImGui::Checkbox("Show Dynamic Cover Query Rays", &navigationDebug.drawCoverQueries);
    ImGui::Checkbox("All NPCs (not just selected)", &navigationDebug.allNpcs);
    if (ImGui::Button("Rebuild Navigation Meshes")) NpcNavMeshSystem::InvalidateAll();
    ImGui::Checkbox("Show Weapon Shot Rays", &m_debugSettings.showWeaponShotRays);
    ImGui::Checkbox("Show Gameplay Particles", &m_debugSettings.showGameplayParticles);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ==================================================================
    // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    // ==================================================================
    ImGui::TextColored(COL_TEXT_DIM, " Collider Type Filters:");

    ImGui::BeginDisabled(!m_debugSettings.showColliders);

    ImGui::Columns(2, nullptr, false);
    ImGui::Checkbox("Boxes", &m_debugSettings.showBoxes);
    ImGui::Checkbox("Spheres", &m_debugSettings.showSpheres);
    ImGui::Checkbox("Capsules", &m_debugSettings.showCapsules);
    ImGui::Checkbox("Cylinders", &m_debugSettings.showCylinders);

    ImGui::NextColumn();

    ImGui::Checkbox("Planes", &m_debugSettings.showPlanes);
    ImGui::Checkbox("Convex Hulls", &m_debugSettings.showConvexHulls);
    ImGui::Checkbox("Triangle Meshes", &m_debugSettings.showTriangleMeshes);
    ImGui::Checkbox("GImpact Meshes", &m_debugSettings.showGImpactMeshes);

    ImGui::Columns(1);
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ==================================================================
    // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    // ==================================================================
    if (ImGui::CollapsingHeader("?? Color Settings", ImGuiTreeNodeFlags_None))
    {
        ImGui::Spacing();
        ImGui::TextColored(COL_TEXT_DIM, " Object States:");

        ImGui::ColorEdit4("Dynamic Bodies##CD", &m_debugSettings.colorDynamic.x);
        ImGui::ColorEdit4("Static Bodies##CS", &m_debugSettings.colorStatic.x);
        ImGui::ColorEdit4("Kinematic Bodies##CK", &m_debugSettings.colorKinematic.x);
        ImGui::ColorEdit4("Sleeping Bodies##CSL", &m_debugSettings.colorSleeping.x);
        ImGui::ColorEdit4("Characters##CCH", &m_debugSettings.colorCharacter.x);

        ImGui::Spacing();
        ImGui::TextColored(COL_TEXT_DIM, " Debug Elements:");

        ImGui::ColorEdit4("Contacts##CC", &m_debugSettings.colorContact.x);
        ImGui::ColorEdit4("AABB##CAABB", &m_debugSettings.colorAABB.x);
        ImGui::ColorEdit4("Ray Hit##CRH", &m_debugSettings.colorRayHit.x);
        ImGui::ColorEdit4("Ray Miss##CRM", &m_debugSettings.colorRayMiss.x);
        ImGui::ColorEdit4("NPC Visible##CNV", &m_debugSettings.colorNpcSightVisible.x);
        ImGui::ColorEdit4("NPC Blocked##CNB", &m_debugSettings.colorNpcSightBlocked.x);
        ImGui::ColorEdit4("Weapon Hit##CWH", &m_debugSettings.colorWeaponRayHit.x);
        ImGui::ColorEdit4("Weapon Miss##CWM", &m_debugSettings.colorWeaponRayMiss.x);

        ImGui::Spacing();

        if (ImGui::Button("Reset to Default##Colors", ImVec2(-1, 0)))
        {
            m_debugSettings.colorDynamic = { 0.2f, 0.8f, 0.2f, 0.6f };
            m_debugSettings.colorStatic = { 0.8f, 0.2f, 0.2f, 0.6f };
            m_debugSettings.colorKinematic = { 0.2f, 0.5f, 0.9f, 0.6f };
            m_debugSettings.colorSleeping = { 0.5f, 0.5f, 0.5f, 0.4f };
            m_debugSettings.colorCharacter = { 0.9f, 0.3f, 1.0f, 0.9f };
            m_debugSettings.colorContact = { 1.0f, 1.0f, 0.0f, 1.0f };
            m_debugSettings.colorAABB = { 0.9f, 0.6f, 0.1f, 0.3f };
            m_debugSettings.colorRayHit = { 0.1f, 1.0f, 0.2f, 1.0f };
            m_debugSettings.colorRayMiss = { 1.0f, 0.1f, 0.1f, 1.0f };
            m_debugSettings.colorNpcSightVisible = { 0.1f, 1.0f, 0.35f, 1.0f };
            m_debugSettings.colorNpcSightBlocked = { 1.0f, 0.25f, 0.1f, 1.0f };
            m_debugSettings.colorWeaponRayHit = { 1.0f, 0.75f, 0.05f, 1.0f };
            m_debugSettings.colorWeaponRayMiss = { 1.0f, 0.15f, 0.8f, 1.0f };
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ==================================================================
    // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    // ==================================================================
    if (ImGui::CollapsingHeader("?? Rendering Parameters"))
    {
        ImGui::Spacing();
        ImGui::SliderFloat("Line Thickness", &m_debugSettings.lineThickness, 0.5f, 5.0f);
        ImGui::SliderFloat("Contact Marker Size", &m_debugSettings.contactMarkerSize, 0.05f, 0.5f);
        ImGui::SliderFloat("Velocity Scale", &m_debugSettings.velocityScale, 0.1f, 2.0f);
        ImGui::SliderFloat("Interaction Ray Lifetime", &m_debugSettings.interactionRayLifetime, 0.1f, 5.0f);
        ImGui::SliderFloat("NPC Sight Ray Lifetime", &m_debugSettings.npcSightRayLifetime, 0.01f, 1.0f);
        ImGui::SliderFloat("Weapon Ray Lifetime", &m_debugSettings.weaponRayLifetime, 0.1f, 5.0f);
        ImGui::Checkbox("Detailed Mesh Wireframes", &m_debugSettings.showComplexMeshWireframes);
        ImGui::SliderInt("Max Mesh Triangles", &m_debugSettings.maxMeshTriangles, 100, 20000);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ==================================================================
    // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    // ==================================================================
    ImGui::TextColored(COL_TEXT_DIM, " Quick Actions:");

    if (ImGui::Button("Enable All##PD", ImVec2(-1, 0)))
    {
        m_debugSettings.showBoxes = m_debugSettings.showSpheres = m_debugSettings.showCapsules = true;
        m_debugSettings.showCylinders = m_debugSettings.showPlanes = true;
        m_debugSettings.showConvexHulls = m_debugSettings.showTriangleMeshes = true;
        m_debugSettings.showGImpactMeshes = true;
    }

    if (ImGui::Button("Disable All##PD", ImVec2(-1, 0)))
    {
        m_debugSettings.showBoxes = m_debugSettings.showSpheres = m_debugSettings.showCapsules = false;
        m_debugSettings.showCylinders = m_debugSettings.showPlanes = false;
        m_debugSettings.showConvexHulls = m_debugSettings.showTriangleMeshes = false;
        m_debugSettings.showGImpactMeshes = false;
    }

    if (ImGui::Button("Primitives Only##PD", ImVec2(-1, 0)))
    {
        m_debugSettings.showBoxes = m_debugSettings.showSpheres = m_debugSettings.showCapsules = true;
        m_debugSettings.showCylinders = true;
        m_debugSettings.showConvexHulls = m_debugSettings.showTriangleMeshes = false;
        m_debugSettings.showGImpactMeshes = false;
    }
    if (ImGui::Button("Performance Profile##PD", ImVec2(-1, 0)))
    {
        m_debugSettings.showColliders = false;
        m_debugSettings.showContacts = false;
        m_debugSettings.showAABB = false;
        m_debugSettings.showCenterOfMass = false;
        m_debugSettings.showVelocity = false;
        m_debugSettings.showInteractionRays = false;
        m_debugSettings.showNpcSightRays = false;
        m_debugSettings.showWeaponShotRays = false;
        m_debugSettings.showComplexMeshWireframes = false;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ==================================================================
    // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦)
    // ==================================================================
    if (ImGui::CollapsingHeader("?? Debug Info"))
    {
        ImGui::Spacing();
        ImGui::TextColored(COL_TEXT_DIM, " Settings Status:");

        ImGui::Text("showColliders: %s", m_debugSettings.showColliders ? "ON" : "OFF");
        ImGui::Text("showBoxes: %s", m_debugSettings.showBoxes ? "ON" : "OFF");
        ImGui::Text("showSpheres: %s", m_debugSettings.showSpheres ? "ON" : "OFF");

        ImGui::Spacing();
        ImGui::TextColored(COL_TEXT_DIM, " Color Values:");
        ImGui::Text("Dynamic: (%.2f, %.2f, %.2f, %.2f)",
            m_debugSettings.colorDynamic.x,
            m_debugSettings.colorDynamic.y,
            m_debugSettings.colorDynamic.z,
            m_debugSettings.colorDynamic.w);
    }
}



// ============================================================================
// Helper: Create a new compartment entity linked to a submarine root
// ============================================================================
// ============================================================================
// Helper: Create a new compartment entity linked to a submarine root
// ============================================================================
void EditorUI::CreateNewCompartment(ECSRegistry& registry, Entity submarineRoot)
{
    Entity newComp = SubmarineSystem::AddCompartmentToSubmarine(
        registry,
        submarineRoot,
        XMFLOAT3(0, 0, 3),        // localOffset
        XMFLOAT3(3, 2.5f, 3.5f),  // compartmentSize
        500.0f                      // compartmentMass
    );

    if (newComp != INVALID_ENTITY)
    {
        // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
        m_selectedType = SelectedType::Object;
        m_selectedEntity = newComp;
    }
}

void EditorUI::StartCompartmentEdit(Entity compartment)
{
    m_editingCompartment = compartment;

    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦, Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦, Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦)
    // Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ registry
    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ UI, Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦

    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ UpdateCacheFromCompartment Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦,
    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
}

// ============================================================================
// UpdateCacheFromCompartment Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦: Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ > Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
// ============================================================================
void EditorUI::UpdateCacheFromCompartment(const CompartmentComponent* comp)
{
    if (!comp) return;

    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Min/Max > Center/Size
    m_compartmentCenter = {
        (comp->localMin.x + comp->localMax.x) * 0.5f,
        (comp->localMin.y + comp->localMax.y) * 0.5f,
        (comp->localMin.z + comp->localMax.z) * 0.5f
    };

    m_compartmentSize = {
        comp->localMax.x - comp->localMin.x,
        comp->localMax.y - comp->localMin.y,
        comp->localMax.z - comp->localMin.z
    };
}

// ============================================================================
// UpdateCompartmentFromCache Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦: Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ > Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
// ============================================================================
void EditorUI::UpdateCompartmentFromCache(CompartmentComponent* comp)
{
    if (!comp) return;

    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Center/Size > Min/Max
    comp->localMin = {
        m_compartmentCenter.x - m_compartmentSize.x * 0.5f,
        m_compartmentCenter.y - m_compartmentSize.y * 0.5f,
        m_compartmentCenter.z - m_compartmentSize.z * 0.5f
    };

    comp->localMax = {
        m_compartmentCenter.x + m_compartmentSize.x * 0.5f,
        m_compartmentCenter.y + m_compartmentSize.y * 0.5f,
        m_compartmentCenter.z + m_compartmentSize.z * 0.5f
    };

    comp->RecalculateVolume();
}





// ============================================================================
// AddCompartmentToSubmarine Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
// ============================================================================
// ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦)
Entity EditorUI::CreateEmptySubmarine(SceneManager& sceneManager, const XMFLOAT3& position)
{
    auto& registry = sceneManager.GetECSRegistry();

    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    Entity subRoot = registry.CreateEntity();

    // Transform
    registry.AddComponent<TransformComponent>(subRoot, TransformComponent{
        .position = position,
        .rotation = {0, 0, 0},
        .scale = {1, 1, 1}
        });

    // RigidBody (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ placeholder)
    RigidBodyComponent rb{};
    rb.mass = 3000.0f;  // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    rb.shapeType = RigidBodyComponent::Sphere;
    rb.shapeRadius = 2.5f;
    rb.isDynamic = true;
    registry.AddComponent<RigidBodyComponent>(subRoot, std::move(rb));
    PhysicsSystem::CreateRigidBody(registry, subRoot);

    // Buoyancy
    registry.AddComponent<BuoyancyComponent>(subRoot, BuoyancyComponent{
        .fluidDensity = 1025.0f,
        .volume = 15.0f,  // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
        .height = 5.0f,
        .buoyancyMultiplier = 1.0f
        });

    // SubmarineComponent (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦!)
    registry.AddComponent<SubmarineComponent>(subRoot, SubmarineComponent{
        .submarineRoot = subRoot
        });

    // Mesh (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ - Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦)
    AssimpModelClass* subModel = sceneManager.GetOrCreateModel(
        "assets/model/sphere.obj", m_device, m_context, "assets/texture/stone.tga");

    if (subModel) {
        registry.AddComponent<MeshRendererComponent>(subRoot, MeshRendererComponent{
            .model = subModel,
            .material = nullptr
            });
    }

    return subRoot;
}

// ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦)
Entity EditorUI::AddCompartmentToSubmarine(SceneManager& sceneManager,
    Entity submarineRoot,
    const XMFLOAT3& localOffset,
    const XMFLOAT3& size,
    float mass)
{
    auto& registry = sceneManager.GetECSRegistry();

    Entity newComp = SubmarineSystem::AddCompartmentToSubmarine(
        registry,
        submarineRoot,
        localOffset,
        size,
        mass
    );

    if (newComp != INVALID_ENTITY)
    {
        // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
        auto* rootSub = registry.GetComponent<SubmarineComponent>(submarineRoot);
        if (rootSub)
        {
            rootSub->needsPhysicsUpdate = true;
        }

        // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦)
        m_selectedType = SelectedType::Object;
        m_selectedEntity = newComp;
    }

    return newComp;
}


// ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
Entity EditorUI::AddCompartmentWithMesh(
    SceneManager& sceneManager,
    Entity submarineRoot,
    const XMFLOAT3& localOffset,
    const XMFLOAT3& size,
    float mass,
    const char* modelPath)
{
    auto& registry = sceneManager.GetECSRegistry();

    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ SubmarineSystem Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    Entity compartment = SubmarineSystem::AddCompartmentToSubmarine(
        registry,
        submarineRoot,
        localOffset,
        size,
        mass
    );

    if (compartment == INVALID_ENTITY)
        return INVALID_ENTITY;

    // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦: Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦!
    AssimpModelClass* compModel = sceneManager.GetOrCreateModel(
        modelPath, m_device, m_context, "assets/texture/stone.tga");

    if (compModel)
    {
        registry.AddComponent<MeshRendererComponent>(compartment, MeshRendererComponent{
            .model = compModel,
            .material = nullptr
            });
    }

    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    auto* rootSub = registry.GetComponent<SubmarineComponent>(submarineRoot);
    if (rootSub)
    {
        rootSub->needsPhysicsUpdate = true;
    }

    return compartment;
}


Entity EditorUI::CreateSubmarineWithMesh(
    SceneManager& sceneManager,
    const XMFLOAT3& position,
    const char* modelPath,
    float hullRadius,
    float hullMass)
{
    auto& registry = sceneManager.GetECSRegistry();

    // 1. Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    Entity subRoot = registry.CreateEntity();

    // Transform
    registry.AddComponent<TransformComponent>(subRoot, TransformComponent{
        .position = position,
        .rotation = {0, 0, 0},
        .scale = {hullRadius, hullRadius, hullRadius}
        });

    // RigidBody
    RigidBodyComponent rb{};
    rb.mass = hullMass;
    rb.shapeType = RigidBodyComponent::Sphere;
    rb.shapeRadius = hullRadius;
    rb.isDynamic = true;
    registry.AddComponent<RigidBodyComponent>(subRoot, std::move(rb));
    PhysicsSystem::CreateRigidBody(registry, subRoot);

    // Buoyancy
    registry.AddComponent<BuoyancyComponent>(subRoot, BuoyancyComponent{
        .fluidDensity = 1025.0f,
        .volume = 15.0f * (hullRadius / 3.0f),
        .height = hullRadius * 2.0f,
        .buoyancyMultiplier = 1.0f
        });

    // SubmarineComponent
    registry.AddComponent<SubmarineComponent>(subRoot, SubmarineComponent{
        .submarineRoot = subRoot
        });

    // ?? Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦: Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦!
    AssimpModelClass* hullModel = sceneManager.GetOrCreateModel(
        modelPath, m_device, m_context, "assets/texture/stone.tga");

    if (hullModel)
    {
        registry.AddComponent<MeshRendererComponent>(subRoot, MeshRendererComponent{
            .model = hullModel,
            .material = nullptr
            });
    }

    return subRoot;
}


void EditorUI::UpdateBreachesOnResize(CompartmentComponent* comp, const XMFLOAT3& oldMin, const XMFLOAT3& oldMax)
{
    if (!comp) return;
    float oldHeight = oldMax.y - oldMin.y;
    float newHeight = comp->localMax.y - comp->localMin.y;
    float scaleY = (oldHeight > 0.0f) ? (newHeight / oldHeight) : 1.0f;

    for (auto& breach : comp->breaches)
    {
        // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Y-Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦)
        float relY = (breach.localPosition.y - oldMin.y) / oldHeight;
        breach.localPosition.y = comp->localMin.y + relY * newHeight;
        // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ X Р С—РЎвЂ”Р вЂ¦ Z, Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
        float oldWidth = oldMax.x - oldMin.x;
        float newWidth = comp->localMax.x - comp->localMin.x;
        float relX = (breach.localPosition.x - oldMin.x) / oldWidth;
        breach.localPosition.x = comp->localMin.x + relX * newWidth;

        float oldDepth = oldMax.z - oldMin.z;
        float newDepth = comp->localMax.z - comp->localMin.z;
        float relZ = (breach.localPosition.z - oldMin.z) / oldDepth;
        breach.localPosition.z = comp->localMin.z + relZ * newDepth;
    }
}



// Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ SubmarineCollisionSetup Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦.
// Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦: registry.HasComponent / GetComponent, ImGui Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦.
void EditorUI::DrawSubmarineCollisionSetup(ECSRegistry& registry, Entity entity)
{
    if (!registry.HasComponent<SubmarineCollisionSetup>(entity))
        return;

    auto* scs = registry.GetComponent<SubmarineCollisionSetup>(entity);

    if (!ImGui::CollapsingHeader("Submarine Collision", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦):
    // 1. Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ useInteriorCollider Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ SubmarineCollisionSetup* Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦).
    // 2. Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ -> Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦.
    // 3. Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦:
    //    ImGui::Checkbox("Use interior collider", &scs.useInteriorCollider);
    //    Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    //    ImGui::Checkbox("Use interior collider", &scs->useInteriorCollider);

    ImGui::Checkbox("Use interior collider", &scs->useInteriorCollider);
    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦/Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    

    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦) Р С—РЎвЂ”Р вЂ¦ DragFloat3 Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    float size[3] = { scs->interiorSize.x, scs->interiorSize.y, scs->interiorSize.z };
    if (ImGui::DragFloat3("Interior size (m)", size, 0.05f, 0.01f, 1000.0f, "%.3f"))
    {
        scs->interiorSize.x = size[0];
        scs->interiorSize.y = size[1];
        scs->interiorSize.z = size[2];
    }

    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    float offset[3] = { scs->interiorOffset.x, scs->interiorOffset.y, scs->interiorOffset.z };
    if (ImGui::DragFloat3("Interior offset (m)", offset, 0.01f, -1000.0f, 1000.0f, "%.3f"))
    {
        scs->interiorOffset.x = offset[0];
        scs->interiorOffset.y = offset[1];
        scs->interiorOffset.z = offset[2];
    }

    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (0.1..1.0)
    float shrink = scs->interiorShrinkFactor;
    if (ImGui::DragFloat("Shrink factor", &shrink, 0.005f, 0.5f, 1.0f, "%.3f"))
    {
        scs->interiorShrinkFactor = std::clamp(shrink, 0.1f, 1.0f);
    }

    // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
    if (ImGui::Button("Default 0.95")) scs->interiorShrinkFactor = 0.95f;
    ImGui::SameLine();
    if (ImGui::Button("Slight -5%")) scs->interiorShrinkFactor = std::min(0.995f, scs->interiorShrinkFactor * 0.95f);
}
void EditorUI::RenderWorldAreaSpawner(SceneManager& sceneManager)
{
    ImGui::SeparatorText("World Areas");
    ImGui::TextWrapped(
        "Create editable world volumes at the current Spawn position, or next to the selected entity.");

    if (ImGui::BeginTable("##WorldAreaPresets", 2, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextColumn();
        if (ImGui::Button("Trigger Zone", ImVec2(-1.0f, 0.0f)))
            CreateEditorTemplate(sceneManager, QuickCreateKind::TriggerArea);
        ImGui::TableNextColumn();
        if (ImGui::Button("Spawn Area", ImVec2(-1.0f, 0.0f)))
            CreateEditorTemplate(sceneManager, QuickCreateKind::SpawnArea);

        ImGui::TableNextColumn();
        if (ImGui::Button("Grass Area", ImVec2(-1.0f, 0.0f)))
            CreateEditorTemplate(sceneManager, QuickCreateKind::GrassArea);
        ImGui::TableNextColumn();
        if (ImGui::Button("No Grass Area", ImVec2(-1.0f, 0.0f)))
            CreateEditorTemplate(sceneManager, QuickCreateKind::NoGrassArea);

        ImGui::TableNextColumn();
        if (ImGui::Button("Forest Area", ImVec2(-1.0f, 0.0f)))
            CreateEditorTemplate(sceneManager, QuickCreateKind::ForestArea);
        ImGui::TableNextColumn();
        if (ImGui::Button("Rebuild Grass", ImVec2(-1.0f, 0.0f)))
        {
            m_rebuildGrassRequested = true;
            SetEditorStatus("Grass rebuild requested");
        }

        ImGui::EndTable();
    }
}

