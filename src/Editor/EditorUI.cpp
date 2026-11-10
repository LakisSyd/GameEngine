#include "EditorUI.h"
#include <string>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <map>
#include <vector>
#include "SceneManager.h"
#include "AssetsLoadSystem.h"
#include "Gameplay/Lighting/SpotLightComponent.h"
#include "CameraController.h"
#include "ObjectDebugWindow.h"
#include <backends/imgui_impl_win32.h>
#include <imgui-master/backends/imgui_impl_dx11.h>
#include "PhysicsSystem.h"
#include "SubmarineJointSystem.h"
#include "FloodingSystem.h"
#include "WeaponComponent.h"
#include "Gameplay/Combat/WeaponSystem.h"
#include "JointConnectionSystem.h"
#include "SubmarineSystem.h"
#include "CharacterSpawnSystem.h"
#include "CharacterControllerSystem.h"
#include "Gameplay/Player/CharacterPhysicsTuningComponent.h"
#include "../../NPCComponents.h"
#include "../../NpcCommandSystem.h"
#include "Gameplay/AI/NpcNavMeshSystem.h"

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
static constexpr ImVec4 COL_RED = ImVec4(0.75f, 0.18f, 0.18f, 1.0f);
static constexpr ImVec4 COL_TEXT_DIM = ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
static constexpr ImVec4 COL_SELECTED = ImVec4(0.06f, 0.45f, 0.80f, 0.35f);
static constexpr ImVec4 COL_X = ImVec4(0.86f, 0.27f, 0.27f, 1.0f);
static constexpr ImVec4 COL_Y = ImVec4(0.27f, 0.72f, 0.27f, 1.0f);
static constexpr ImVec4 COL_Z = ImVec4(0.27f, 0.47f, 0.86f, 1.0f);
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
    default: return "Unknown";
    }
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

static void DrawWeaponGripEditor(WeaponComponent& weapon)
{
    if (!ImGui::CollapsingHeader(
        "Weapon Grip Editor##weapon_grip",
        ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    ImGui::SeparatorText("Visual asset and animation");
    InputStringValue("Model Path##weapon_grip", weapon.visualModelPath);
    InputStringValue("Right Hand Bone##weapon_grip", weapon.rightHandBone);
    InputStringValue("Rifle Idle Clip##weapon_grip", weapon.rifleIdleAnimation);
    InputStringValue("Rifle Gun Clip##weapon_grip", weapon.rifleGunAnimation);
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
    if (ImGui::BeginTable(
        "WeaponGripTransformGrid",
        3,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 95.0f);
        ImGui::TableSetupColumn("World");
        ImGui::TableSetupColumn("First Person");
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

        ImGui::EndTable();
    }

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
    ImGui::Checkbox("Enable Left Hand IK##weapon_grip", &weapon.enableLeftHandIK);
    ImGui::BeginDisabled(!weapon.enableLeftHandIK);
    ImGui::DragFloat3(
        "Left Hand Grip##weapon_grip",
        &weapon.leftHandGrip.x,
        0.005f);
    ImGui::EndDisabled();
    ImGui::TextDisabled("Left Hand Grip is saved for the upcoming IK pass.");
    ImGui::TextDisabled(
        "Transforms update live in Play. Model path and visual toggles apply on next Play.");
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

        {
            char status[96];
            snprintf(status, sizeof(status), " %zu obj  |  %d lights  |  %d sprites  ",
                sceneManager.GetObjectCount(),
                NUM_LIGHTS,
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
    RenderMainMenu(sceneManager);
    RenderSidebar(sceneManager);
    RenderCompartmentEditor(sceneManager);
    RenderDetailsPanel(sceneManager);
    RenderScenePreview();
    RenderAssetLoadingPanel(sceneManager);
    EditorDebug::RenderObjectDebugWindow(sceneManager, m_selectedEntity, m_showObjectDebugWindow);
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

    ImGui::SetNextWindowSize(ImVec2(390.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("NPC Debug Control"))
    {
        ImGui::End();
        return;
    }

    ImGui::TextColored(
        ImVec4(0.35f, 0.85f, 1.0f, 1.0f),
        "F2: toggle mouse between camera and debug UI");
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
    if (ImGui::Button("Rebuild all room NavMeshes", ImVec2(-1.0f, 0.0f)))
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

void EditorUI::UpdateDebugOverlay(SceneManager& sceneManager)
{
    NewFrame();
    EditorDebug::RenderObjectDebugWindow(sceneManager, m_selectedEntity, m_showObjectDebugWindow);
    RenderNpcPlayDebugWindow(sceneManager, m_selectedEntity);
}

void EditorUI::UpdatePlayOverlay(SceneManager& sceneManager)
{
    NewFrame();
    EditorDebug::RenderObjectDebugWindow(
        sceneManager, m_selectedEntity, m_showObjectDebugWindow);
    RenderNpcPlayDebugWindow(sceneManager, m_selectedEntity);
    RenderWeaponGripPlayWindow(sceneManager);
}

void EditorUI::RenderWeaponGripPlayWindow(SceneManager& sceneManager)
{
    auto& registry = sceneManager.GetECSRegistry();
    Entity player = INVALID_ENTITY;
    registry.ForEach<PlayerGameTag>([&](Entity candidate, PlayerGameTag&)
    {
        if (player == INVALID_ENTITY)
            player = candidate;
    });

    ImGui::SetNextWindowPos(ImVec2(24.0f, 72.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(760.0f, 720.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Weapon Grip (Play Mode)"))
    {
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("F2: release/capture mouse");
    if (player == INVALID_ENTITY)
    {
        ImGui::TextColored(COL_RED, "PlayerGameTag was not found.");
        ImGui::End();
        return;
    }

    const Entity weaponEntity =
        WeaponSystem::GetEquippedWeaponEntity(registry, player);
    auto* weapon = registry.GetComponent<WeaponComponent>(weaponEntity);
    if (!weapon)
    {
        ImGui::TextColored(COL_RED, "Equipped weapon was not found.");
        ImGui::End();
        return;
    }

    const float buttonWidth =
        (ImGui::GetContentRegionAvail().x - 16.0f) / 3.0f;
    if (ImGui::Button("Save Profile", ImVec2(buttonWidth, 0.0f)))
    {
        const bool saved = sceneManager.SaveWeaponProfile(*weapon);
        m_weaponProfileStatus = saved
            ? "Saved: assets/config/weapon_grip.json"
            : "Could not save weapon profile.";
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload Profile", ImVec2(buttonWidth, 0.0f)))
    {
        const bool loaded = sceneManager.LoadWeaponProfile(*weapon);
        m_weaponProfileStatus = loaded
            ? "Profile reloaded."
            : "No saved profile was found.";
        if (loaded)
            m_weaponVisualReloadRequested = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload Visual", ImVec2(-1.0f, 0.0f)))
    {
        m_weaponVisualReloadRequested = true;
        m_weaponProfileStatus = "Weapon visual reloaded.";
    }

    if (!m_weaponProfileStatus.empty())
        ImGui::TextWrapped("%s", m_weaponProfileStatus.c_str());

    DrawWeaponGripEditor(*weapon);
    ImGui::End();
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

    const float avail = ImGui::GetContentRegionAvail().y - std::floor(4.0f * ScaleY());
    ImGui::BeginChild("##OutlinerList", ImVec2(-1, avail), false);

    auto& registry = sceneManager.GetECSRegistry();

    auto drawSelectableEntity = [&](const char* label, Entity entity, SelectedType type)
    {
        const bool selected = (m_selectedType == type && m_selectedEntity == entity);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Header, COL_SELECTED);
        if (ImGui::Selectable(label, selected))
            SetSelection(type, entity);
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
        if (!sceneManager.IsObjectActive(entity)) continue;

        if (!panel.roomName.empty())
            panelsByRoom[panel.roomName].push_back(entity);
        else
            ungroupedPanels.push_back(entity);
    }

    // -- Mesh objects, except hull panels. Panels are rendered under rooms below.
    auto& meshPool = registry.GetComponents<MeshRendererComponent>();
    for (auto& [entity, mesh] : meshPool)
    {
        if (!sceneManager.IsObjectActive(entity)) continue;
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
        if (!sceneManager.IsObjectActive(roomEntity)) continue;
        if (registry.HasComponent<MeshRendererComponent>(roomEntity)) continue;

        auto panelIt = panelsByRoom.find(room.roomName);
        const int panelCount = panelIt != panelsByRoom.end()
            ? static_cast<int>(panelIt->second.size())
            : 0;

        char roomLabel[128];
        snprintf(roomLabel, sizeof(roomLabel), "Room: %s [%u]  Panels: %d", room.roomName.c_str(), roomEntity, panelCount);

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
        if (!sceneManager.IsObjectActive(entity)) continue;
        if (registry.HasComponent<MeshRendererComponent>(entity)) continue;

        const char* typeName = "World";
        if (area.type == WorldAreaType::Grass) typeName = "Grass";
        if (area.type == WorldAreaType::NoGrass) typeName = "No Grass";
        if (area.type == WorldAreaType::Forest) typeName = "Forest";
        if (area.type == WorldAreaType::Spawn) typeName = "Spawn";
        if (area.type == WorldAreaType::Trigger) typeName = "Trigger";

        char label[64];
        snprintf(label, sizeof(label), "  Area: %s [%u]", typeName, entity);
        drawSelectableEntity(label, entity, SelectedType::Object);
    }

    // -- Collision-only entities ---------------------------------------------
    auto& rbPool = registry.GetComponents<RigidBodyComponent>();
    for (auto& [entity, rb] : rbPool)
    {
        if (!sceneManager.IsObjectActive(entity)) continue;
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

    ImGui::Spacing();

    for (int i = 0; i < sceneManager.GetSpriteCount(); ++i)
    {
        char label[64];
        snprintf(label, sizeof(label), "  Sprite #%d", i);

        const bool selected = (m_selectedType == SelectedType::Sprite && m_selectedLegacyIndex == i);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Header, COL_SELECTED);
        if (ImGui::Selectable(label, selected))
            SetSelection(SelectedType::Sprite, i);
        if (selected) ImGui::PopStyleColor();
    }

    ImGui::EndChild();
}
//=============================================================================
void EditorUI::RenderDetailsPanel(SceneManager& sceneManager)
{
    if (m_selectedType == SelectedType::None) return;

    ImGui::Begin("Details", nullptr, ImGuiWindowFlags_NoCollapse);

    switch (m_selectedType)
    {
    case SelectedType::Object:
        if (m_selectedEntity != INVALID_ENTITY)
            RenderObjectProperties(sceneManager, m_selectedEntity);
        break;

    case SelectedType::Light:
        if (m_selectedEntity != INVALID_ENTITY)
            RenderLightPropertiesByEntity(sceneManager, m_selectedEntity);
        break;

    case SelectedType::Sprite:
        // ...
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
    auto* transform = sceneManager.GetObjectTransform(entity);
    if (!transform)
    {
        if (registry.HasComponent<WorldAreaComponent>(entity))
        {
            RenderComponentManager(sceneManager, entity);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button("Remove Area", ImVec2(-1, 0)))
            {
                sceneManager.RemoveObject(entity);
                m_selectedEntity = INVALID_ENTITY;
                m_selectedType = SelectedType::None;
            }
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
    if (DragFloat3Colored("##ObjPos", pos, 0.1f))
        transform->position = { pos[0], pos[1], pos[2] };

    ImGui::Spacing();

    float rot[3] = { transform->rotation.x, transform->rotation.y, transform->rotation.z };
    ImGui::TextColored(COL_TEXT_DIM, " Rotation");
    if (DragFloat3Colored("##ObjRot", rot, 0.01f))
        transform->rotation = { rot[0], rot[1], rot[2] };

    ImGui::Spacing();

    float scl[3] = { transform->scale.x, transform->scale.y, transform->scale.z };
    ImGui::TextColored(COL_TEXT_DIM, " Scale");
    if (DragFloat3Colored("##ObjScl", scl, 0.01f))
        transform->scale = { scl[0], scl[1], scl[2] };

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ --------------------------------------------------
    RenderComponentManager(sceneManager, entity);



    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // -- Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ --------------------------------------------------------
    ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    if (ImGui::Button("Remove Object", ImVec2(-1, 0)))
    {
        sceneManager.RemoveObject(entity);

        m_selectedEntity = INVALID_ENTITY;
        m_selectedType = SelectedType::None;
    }
    ImGui::PopStyleColor(2);
}

//=============================================================================
// RenderComponentManager Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦, Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦
//                          Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ UI
//=============================================================================
void EditorUI::RenderComponentManager(SceneManager& sceneManager, Entity entity)
{
    auto& registry = sceneManager.GetECSRegistry();

    if (auto* npc = registry.GetComponent<NPCComponent>(entity))
    {
        auto* brain = registry.GetComponent<NpcBrainComponent>(entity);
        auto* movement = registry.GetComponent<NPCMovementComponent>(entity);

        if (ImGui::CollapsingHeader("NPC AI", ImGuiTreeNodeFlags_DefaultOpen))
        {
            char name[128]{};
            char description[256]{};
            strncpy_s(name, npc->name.c_str(), _TRUNCATE);
            strncpy_s(description, npc->description.c_str(), _TRUNCATE);
            if (ImGui::InputText("Name##npc", name, sizeof(name))) npc->name = name;
            if (ImGui::InputTextMultiline("Description##npc", description, sizeof(description), ImVec2(-1.0f, 60.0f))) npc->description = description;
            ImGui::DragFloat("Health##npc", &npc->health, 1.0f, 0.0f, npc->maxHealth);
            ImGui::DragFloat("Max health##npc", &npc->maxHealth, 1.0f, 1.0f, 10000.0f);
            npc->health = (std::min)(npc->health, npc->maxHealth);
            ImGui::Checkbox("Hostile##npc", &npc->isHostile);

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

                if (ImGui::Button("Rebuild all room NavMeshes##npc", ImVec2(-1.0f, 0.0f)))
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

            if (auto* animation = registry.GetComponent<AnimationComponent>(entity))
            {
                ImGui::SeparatorText("Animation Bindings");
                ImGui::Text("Current: %s", AnimationStateName(animation->currentState));
                for (auto& binding : animation->animations)
                {
                    ImGui::PushID(static_cast<int>(binding.state));
                    char clipName[128]{};
                    strncpy_s(clipName, binding.name.c_str(), _TRUNCATE);
                    if (ImGui::InputText(AnimationStateName(binding.state), clipName, sizeof(clipName)))
                    {
                        binding.name = clipName;
                        binding.index = -1;
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("index %d", binding.index);
                    ImGui::PopID();
                }
                ImGui::TextDisabled("Walk/Run fall back to each other; missing Idle pauses playback.");
            }
            else
            {
                ImGui::TextColored(COL_RED, "Missing AnimationComponent");
            }

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button("Remove NPC AI", ImVec2(-1.0f, 0.0f)))
            {
                registry.RemoveComponent<NPCComponent>(entity);
                registry.RemoveComponent<NpcBrainComponent>(entity);
                registry.RemoveComponent<NPCMovementComponent>(entity);
            }
            ImGui::PopStyleColor(2);
        }
    }

    // One common physics panel appears for every character entity. This keeps
    // Player and NPC tuning in sync and avoids two subtly different inspectors.
    if (registry.HasComponent<CharacterController>(entity))
    {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.24f, 0.16f, 0.05f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.36f, 0.23f, 0.07f, 1.0f));
        const bool physicsOpen = ImGui::CollapsingHeader(
            "Character Physics (Player + NPC)", ImGuiTreeNodeFlags_DefaultOpen);
        ImGui::PopStyleColor(2);
        if (physicsOpen)
            RenderCharacterPhysicsTuning(registry, entity);
    }


    if (registry.HasComponent<WorldAreaComponent>(entity))
    {
        auto* area = registry.GetComponent<WorldAreaComponent>(entity);

        if (area)
        {
            const char* areaTypes[] = { "Grass", "No Grass", "Forest", "Spawn", "Trigger" };
            int type = static_cast<int>(area->type);

            ImGui::SeparatorText("World Area");
            if (ImGui::Combo("Type", &type, areaTypes, IM_ARRAYSIZE(areaTypes)))
                area->type = static_cast<WorldAreaType>(type);

            ImGui::DragFloat3("Center", &area->center.x, 0.1f);
            ImGui::DragFloat3("Size", &area->size.x, 0.1f, 0.1f, 500.0f);
            ImGui::DragFloat("Density", &area->density, 0.01f, 0.0f, 20.0f);
            ImGui::InputScalar("Seed", ImGuiDataType_U32, &area->seed);
            ImGui::Checkbox("Enabled", &area->enabled);
        }
    }

    // -- HullPanelComponent ----------------------------------------------------
    if (auto* panel = registry.GetComponent<HullPanelComponent>(entity))
    {
        if (ImGui::CollapsingHeader("Hull Panel Component", ImGuiTreeNodeFlags_DefaultOpen))
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
    // -- InteractionComponent --------------------------------------------------
    if (auto* interaction = registry.GetComponent<InteractionComponent>(entity))
    {
        if (ImGui::CollapsingHeader("Interaction", ImGuiTreeNodeFlags_DefaultOpen))
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
        bool open = ImGui::CollapsingHeader("  Door Component##door", ImGuiTreeNodeFlags_DefaultOpen);
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
                WeaponComponent weaponTemplate{};
                if (auto* legacyWeapon = registry.GetComponent<WeaponComponent>(entity))
                    weaponTemplate = *legacyWeapon;
                createEquippedWeapon(entity, weaponTemplate);
            }

            if (registry.HasComponent<WeaponComponent>(entity))
                registry.RemoveComponent<WeaponComponent>(entity);
        };

        const bool isPlayer = registry.HasComponent<PlayerGameTag>(entity);
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.06f, 0.20f, 0.32f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.08f, 0.30f, 0.45f, 1.0f));
        bool playerOpen = ImGui::CollapsingHeader("  Player##player", isPlayer ? ImGuiTreeNodeFlags_DefaultOpen : 0);
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
                    ImGui::Spacing();
                    ImGui::DragFloat("Health", &player->health, 1.0f, 0.0f, 100.0f);
                    ImGui::DragFloat("Stamina", &player->stamina, 1.0f, 0.0f, 100.0f);
                    ImGui::DragFloat("Oxygen", &player->oxygen, 1.0f, 0.0f, 100.0f);
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
                    if (auto* player = registry.GetComponent<PlayerComponent>(entity))
                        ImGui::Text("Stamina: %.1f / %.1f", player->stamina, player->maxStamina);
                }
                else
                {
                    ImGui::TextColored(COL_RED, "Missing CharacterMovement");
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

                    DrawWeaponGripEditor(*weapon);
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
        if (ImGui::CollapsingHeader(
            "  First Person View##fp_view",
            ImGuiTreeNodeFlags_DefaultOpen))
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
        if (ImGui::CollapsingHeader(
            "  Bone Attachment##bone_attachment",
            ImGuiTreeNodeFlags_DefaultOpen))
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
        bool open = ImGui::CollapsingHeader("  Light Component##lc", ImGuiTreeNodeFlags_DefaultOpen);
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
        bool open = ImGui::CollapsingHeader("  Damageable Component##dc", ImGuiTreeNodeFlags_DefaultOpen);
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
        bool open = ImGui::CollapsingHeader("  Submarine Component##sub", ImGuiTreeNodeFlags_DefaultOpen);
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
        bool open = ImGui::CollapsingHeader("  RigidBody Component##rbc", ImGuiTreeNodeFlags_DefaultOpen);
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
        bool open = ImGui::CollapsingHeader("  Buoyancy Component##buoy", ImGuiTreeNodeFlags_DefaultOpen);
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
        ImGui::Separator();
        ImGui::Spacing();

        {
            const bool hasNpc = registry.HasComponent<NPCComponent>(entity);
            ImGui::BeginDisabled(hasNpc);
            if (ImGui::Selectable("  NPC AI Components"))
            {
                registry.AddComponent<NPCComponent>(entity, NPCComponent{});
                registry.AddComponent<NpcBrainComponent>(entity, NpcBrainComponent{});
                registry.AddComponent<NPCMovementComponent>(entity, NPCMovementComponent{});
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

        // -- Submarine Component --------------------------------------------------
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
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
        }

        const bool weaponHas = registry.HasComponent<WeaponComponent>(entity);
		ImGui::BeginDisabled(weaponHas);
        if (ImGui::Selectable("  Weapon Component"))
        {
            registry.AddComponent<WeaponComponent>(entity, WeaponComponent{});
        }
        ImGui::EndDisabled();

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

		const bool hasDamage = registry.HasComponent<DamageableComponent>(entity);
		ImGui::BeginDisabled(hasDamage);
        if (ImGui::Selectable("  Damageable Component"))
        {
            registry.AddComponent<DamageableComponent>(entity, DamageableComponent{});
            ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();

        // -- Light Component -----------------------------------------------------
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

        ImGui::Spacing();
        ImGui::EndPopup();
    }
}

void EditorUI::RenderCameraPanel()
{
    SectionHeader("Play Camera");

    if (!m_cameraController)
    {
        ImGui::TextColored(COL_TEXT_DIM, "Camera controller is not available");
        return;
    }

    const char* viewModes[] = { "Third Person", "First Person" };
    int viewMode = m_cameraController->GetPlayViewMode() == CameraController::PlayViewMode::FirstPerson ? 1 : 0;
    if (ImGui::Combo("Play View", &viewMode, viewModes, IM_ARRAYSIZE(viewModes)))
    {
        m_cameraController->SetPlayViewMode(
            viewMode == 1 ? CameraController::PlayViewMode::FirstPerson : CameraController::PlayViewMode::ThirdPerson
        );
    }

    float eyeHeight = m_cameraController->GetFirstPersonEyeHeight();
    if (ImGui::DragFloat("Eye Height", &eyeHeight, 0.01f, 0.1f, 3.0f, "%.2f"))
        m_cameraController->SetFirstPersonEyeHeight(eyeHeight);

    float forwardOffset = m_cameraController->GetFirstPersonForwardOffset();
    if (ImGui::DragFloat("Forward Offset", &forwardOffset, 0.01f, -1.0f, 1.0f, "%.2f"))
        m_cameraController->SetFirstPersonForwardOffset(forwardOffset);

    float distance = m_cameraController->GetThirdPersonDistance();
    if (ImGui::DragFloat("Third Distance", &distance, 0.05f, 1.0f, 30.0f, "%.2f"))
        m_cameraController->SetThirdPersonDistance(distance);

    float pitch = m_cameraController->GetFollowPitch();
    if (ImGui::DragFloat("Pitch", &pitch, 0.1f, -89.0f, 89.0f, "%.1f"))
        m_cameraController->SetFollowPitch(pitch);

    float lookUpLimit = m_cameraController->GetLookUpLimit();
    if (ImGui::SliderFloat("Look Up Limit", &lookUpLimit, 0.0f, 89.0f, "%.1f deg"))
        m_cameraController->SetLookUpLimit(lookUpLimit);

    float lookDownLimit = m_cameraController->GetLookDownLimit();
    if (ImGui::SliderFloat("Look Down Limit", &lookDownLimit, 0.0f, 89.0f, "%.1f deg"))
        m_cameraController->SetLookDownLimit(lookDownLimit);

    ImGui::Spacing();
    ImGui::TextColored(COL_TEXT_DIM, "Used when gameplay camera follows the player.");
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

    const float headerH = std::floor(100.0f * ScaleY());

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.07f, 0.07f, 1.0f));
    ImGui::BeginChild("##SidebarHeader", ImVec2(-1, headerH), false);
    ImGui::SetCursorPos(ImVec2(10, std::floor(9.0f * ScaleY())));
    ImGui::TextColored(COL_ACCENT, "?");
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

        if (ImGui::BeginTabItem("?? Debug"))
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
            RenderCameraPanel();
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
            ImGui::TextColored(COL_ACCENT, "?"); ImGui::SameLine();
            ImGui::Text("Object  entity=%u", m_selectedEntity);
            break;
        case SelectedType::Light:
            ImGui::TextColored(ImVec4(1, 0.9f, 0.3f, 1), "?"); ImGui::SameLine();
            ImGui::Text("Light #%d", m_selectedLegacyIndex);
            break;
        case SelectedType::Sprite:
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 1, 1), "?"); ImGui::SameLine();
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
    {
        // Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ (Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦, Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦ Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦Р С—РЎвЂ”Р вЂ¦)
        registry.DestroyEntity(lightEntity);
    }
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

    if (ImGui::Button("Add Grass Area"))
    {
        Entity entity =
            sceneManager.CreateWorldArea(WorldAreaType::Grass);

        m_selectedType = SelectedType::Object;
        m_selectedEntity = entity;
    }

    if (ImGui::Button("Rebuild Grass"))
    {
        m_rebuildGrassRequested = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("Add No Grass Area"))
    {
        Entity entity =
            sceneManager.CreateWorldArea(WorldAreaType::NoGrass);

        m_selectedType = SelectedType::Object;
        m_selectedEntity = entity;
    }
}

