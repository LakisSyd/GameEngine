#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "AnimationComponent.h"
#include "Json/json.hpp"

namespace MeleeActionJson
{
    using json = nlohmann::json;

    inline std::string ProfilePath(const std::string& name)
    {
        if (name.empty() || name.size() > 64 ||
            !std::all_of(name.begin(), name.end(), [](unsigned char c)
                { return std::isalnum(c) || c == '_' || c == '-'; }))
            return {};
        return (std::filesystem::path("assets/config/animations") /
            (name + ".json")).string();
    }

    inline void Write(json& data, const AnimationComponent& animation,
        const char* variantsKey)
    {
        const auto& actions = animation.meleeActions;
        data["upper_body_root"] = animation.upperBodyRoot;
        data["crouch_pose"] = animation.crouchPose;
        data["actions"] = json::array({
            { { "id", "unarmed_guard" }, { "type", "hold_guard" },
              { "input", "Aim" }, { "enabled", actions.guardEnabled },
              { "world_clip", actions.guardWorldClip },
              { "first_person_clip", actions.guardFirstPersonClip },
              { "move_world_clip", actions.guardMoveWorldClip },
              { "move_first_person_clip", actions.guardMoveFirstPersonClip } },
            { { "id", "unarmed_punch" }, { "type", "weighted_punch" },
              { "input", "PrimaryMouse" },
              { "requires_guard", actions.attackRequiresGuard },
              { "base_world_clip", actions.attackBaseWorldClip } }
        });
        data[variantsKey] = json::array();
        for (const auto& variant : animation.punchVariants)
            data[variantsKey].push_back({
                { "name", variant.name }, { "world_clip", variant.worldClip },
                { "first_person_clip", variant.firstPersonClip },
                { "right_hand", variant.rightHand },
                { "contact_phase", variant.contactPhase },
                { "weight", variant.weight },
                { "ik_strength", variant.ikStrength },
                { "fist_offset", { variant.fistOffset.x,
                    variant.fistOffset.y, variant.fistOffset.z } },
                { "hand_rotation", { variant.handRotation.x,
                    variant.handRotation.y, variant.handRotation.z } },
                { "aim_offset", { variant.aimOffset.x,
                    variant.aimOffset.y, variant.aimOffset.z } },
                { "elbow_pole_offset", { variant.elbowPoleOffset.x,
                    variant.elbowPoleOffset.y, variant.elbowPoleOffset.z } },
                { "arm_extension", variant.armExtension },
                { "ik_start_phase", variant.ikStartPhase },
                { "ik_end_phase", variant.ikEndPhase } });
        data["first_person_clip_offsets"] = json::array();
        for (const auto& offset : animation.firstPersonClipOffsets)
            data["first_person_clip_offsets"].push_back({
                { "clip", offset.clip }, { "variant", offset.variant },
                { "position", { offset.position.x, offset.position.y,
                    offset.position.z } },
                { "rotation", { offset.rotation.x, offset.rotation.y,
                    offset.rotation.z } } });
        data["clip_speeds"] = json::array();
        for (const auto& setting : animation.clipSpeeds)
            if (!setting.clip.empty() && std::isfinite(setting.speed))
                data["clip_speeds"].push_back({
                    { "clip", setting.clip },
                    { "speed", std::clamp(setting.speed, 0.1f, 5.0f) } });
    }

    inline void Read(const json& data, AnimationComponent& animation,
        const char* variantsKey)
    {
        if (auto it = data.find("crouch_pose"); it != data.end() && it->is_boolean())
            animation.crouchPose = it->get<bool>();
        // Profiles and scene loading must restore the same editable state map.
        if (auto it = data.find("animation_bindings"); it != data.end() && it->is_array())
            for (const auto& saved : *it)
            {
                if (!saved.is_object() || !saved.contains("state") ||
                    !saved["state"].is_number_integer() || !saved.contains("name") ||
                    !saved["name"].is_string()) continue;
                for (auto& binding : animation.animations)
                    if (static_cast<int>(binding.state) == saved["state"].get<int>())
                    {
                        binding.name = saved["name"].get<std::string>();
                        binding.index = -1;
                    }
            }
        if (auto it = data.find("upper_body_root");
            it != data.end() && it->is_string() && !it->get_ref<const std::string&>().empty())
            animation.upperBodyRoot = it->get<std::string>();
        if (auto it = data.find("actions"); it != data.end() && it->is_array())
            for (const auto& action : *it)
            {
                if (!action.is_object()) continue;
                const std::string type = action.value("type", std::string{});
                if (type == "hold_guard")
                {
                    animation.meleeActions.guardEnabled = action.value("enabled", true);
                    animation.meleeActions.guardWorldClip = action.value(
                        "world_clip", animation.meleeActions.guardWorldClip);
                    animation.meleeActions.guardFirstPersonClip = action.value(
                        "first_person_clip", animation.meleeActions.guardFirstPersonClip);
                    animation.meleeActions.guardMoveWorldClip = action.value(
                        "move_world_clip", animation.meleeActions.guardMoveWorldClip);
                    animation.meleeActions.guardMoveFirstPersonClip = action.value(
                        "move_first_person_clip", animation.meleeActions.guardMoveFirstPersonClip);
                }
                else if (type == "weighted_punch")
                {
                    animation.meleeActions.attackRequiresGuard = action.value(
                        "requires_guard", animation.meleeActions.attackRequiresGuard);
                    animation.meleeActions.attackBaseWorldClip = action.value(
                        "base_world_clip",
                        animation.meleeActions.attackBaseWorldClip);
                }
            }
        if (auto it = data.find(variantsKey); it != data.end() && it->is_array())
        {
            std::vector<PunchVariant> variants;
            for (const auto& saved : *it)
            {
                if (!saved.is_object()) continue;
                PunchVariant variant;
                variant.name = saved.value("name", std::string{"Punch"});
                variant.worldClip = saved.value("world_clip", std::string{});
                variant.firstPersonClip = saved.value("first_person_clip", std::string{});
                variant.rightHand = saved.value("right_hand", false);
                variant.contactPhase = saved.value("contact_phase", -1.0f);
                variant.weight = saved.value("weight", 1.0f);
                if (!std::isfinite(variant.weight) || variant.weight < 0.0f)
                    variant.weight = 0.0f;
                variant.ikStrength = saved.value("ik_strength", -1.0f);
                if (!std::isfinite(variant.ikStrength))
                    variant.ikStrength = -1.0f;
                if (variant.ikStrength >= 0.0f)
                    variant.ikStrength = std::clamp(variant.ikStrength, 0.0f, 1.0f);
                const auto read3 = [](const json& value,
                    DirectX::XMFLOAT3& output)
                {
                    if (!value.is_array() || value.size() != 3 ||
                        !value[0].is_number() || !value[1].is_number() ||
                        !value[2].is_number()) return;
                    const float x = value[0].get<float>();
                    const float y = value[1].get<float>();
                    const float z = value[2].get<float>();
                    if (std::isfinite(x) && std::isfinite(y) && std::isfinite(z))
                        output = { x, y, z };
                };
                if (saved.contains("fist_offset"))
                    read3(saved["fist_offset"], variant.fistOffset);
                if (saved.contains("hand_rotation"))
                    read3(saved["hand_rotation"], variant.handRotation);
                if (saved.contains("aim_offset"))
                    read3(saved["aim_offset"], variant.aimOffset);
                if (saved.contains("elbow_pole_offset"))
                    read3(saved["elbow_pole_offset"], variant.elbowPoleOffset);
                variant.armExtension = saved.value("arm_extension", 0.92f);
                if (!std::isfinite(variant.armExtension))
                    variant.armExtension = 0.92f;
                variant.armExtension = std::clamp(
                    variant.armExtension, 0.5f, 0.99f);
                variant.ikStartPhase = saved.value("ik_start_phase", 0.0f);
                variant.ikEndPhase = saved.value("ik_end_phase", 1.0f);
                if (!std::isfinite(variant.ikStartPhase))
                    variant.ikStartPhase = 0.0f;
                if (!std::isfinite(variant.ikEndPhase))
                    variant.ikEndPhase = 1.0f;
                variant.ikStartPhase = std::clamp(
                    variant.ikStartPhase, 0.0f, 0.95f);
                variant.ikEndPhase = std::clamp(
                    variant.ikEndPhase, variant.ikStartPhase + 0.05f, 1.0f);
                variants.push_back(std::move(variant));
            }
            if (!variants.empty()) animation.punchVariants = std::move(variants);
        }
        if (auto it = data.find("first_person_clip_offsets");
            it != data.end() && it->is_array())
        {
            std::vector<FirstPersonClipOffset> offsets;
            for (const auto& saved : *it)
            {
                if (!saved.is_object() || !saved.contains("clip") ||
                    !saved["clip"].is_string()) continue;
                FirstPersonClipOffset offset;
                offset.clip = saved["clip"].get<std::string>();
                if (offset.clip.empty()) continue;
                offset.variant = saved.value("variant", -1);
                const auto read3 = [](const json& value, DirectX::XMFLOAT3& output)
                {
                    if (value.is_array() && value.size() == 3 &&
                        value[0].is_number() && value[1].is_number() &&
                        value[2].is_number())
                        output = { value[0].get<float>(), value[1].get<float>(),
                            value[2].get<float>() };
                };
                if (saved.contains("position")) read3(saved["position"], offset.position);
                if (saved.contains("rotation")) read3(saved["rotation"], offset.rotation);
                offsets.push_back(std::move(offset));
            }
            animation.firstPersonClipOffsets = std::move(offsets);
        }
        if (auto it = data.find("clip_speeds");
            it != data.end() && it->is_array())
        {
            std::vector<AnimationClipSpeed> speeds;
            for (const auto& saved : *it)
            {
                if (!saved.is_object() || !saved.contains("clip") ||
                    !saved["clip"].is_string() || !saved.contains("speed") ||
                    !saved["speed"].is_number()) continue;
                AnimationClipSpeed setting;
                setting.clip = saved["clip"].get<std::string>();
                setting.speed = saved["speed"].get<float>();
                if (setting.clip.empty() || !std::isfinite(setting.speed)) continue;
                setting.speed = std::clamp(setting.speed, 0.1f, 5.0f);
                speeds.push_back(std::move(setting));
            }
            animation.clipSpeeds = std::move(speeds);
        }
    }

    inline bool ReadPoseKeys(const json& data, AnimationComponent& animation,
        const char* keyName)
    {
        auto it = data.find(keyName);
        if (it == data.end() || !it->is_array()) return false;
        std::vector<AnimationPoseKey> keys;
        for (const auto& saved : *it)
        {
            if (!saved.is_object() || !saved.contains("clip") ||
                !saved.contains("bone") || !saved["clip"].is_string() ||
                !saved["bone"].is_string()) continue;
            AnimationPoseKey key;
            key.clip = saved["clip"].get<std::string>();
            key.bone = saved["bone"].get<std::string>();
            if (key.clip.empty() || key.bone.empty()) continue;
            if (saved.contains("variant") &&
                !saved["variant"].is_number_integer()) continue;
            key.variant = saved.value("variant", -1);
            if (key.variant < -1) continue;
            if (saved.contains("time") && !saved["time"].is_number())
                continue;
            key.time = saved.value("time", 0.0f);
            if (!std::isfinite(key.time)) continue;
            key.time = (std::max)(0.0f, key.time);
            const auto read3 = [](const json& value, DirectX::XMFLOAT3& output)
            {
                if (!value.is_array() || value.size() != 3 ||
                    !value[0].is_number() || !value[1].is_number() ||
                    !value[2].is_number()) return false;
                const float x = value[0].get<float>();
                const float y = value[1].get<float>();
                const float z = value[2].get<float>();
                if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
                    return false;
                output = { x, y, z };
                return true;
            };
            if (saved.contains("position") &&
                !read3(saved["position"], key.position)) continue;
            if (saved.contains("rotation") &&
                !read3(saved["rotation"], key.rotation)) continue;
            keys.push_back(std::move(key));
        }
        animation.poseKeys = std::move(keys);
        return true;
    }

    inline bool ReadFistKeys(const json& data, AnimationComponent& animation,
        const char* keyName)
    {
        auto it = data.find(keyName);
        if (it == data.end() || !it->is_array()) return false;
        std::vector<AnimationFistKey> keys;
        for (const auto& saved : *it)
        {
            if (!saved.is_object() || !saved.contains("clip") ||
                !saved["clip"].is_string() || !saved.contains("amount") ||
                !saved["amount"].is_number()) continue;
            AnimationFistKey key;
            key.clip = saved["clip"].get<std::string>();
            if (key.clip.empty()) continue;
            if (saved.contains("variant") &&
                !saved["variant"].is_number_integer()) continue;
            key.variant = saved.value("variant", -1);
            if (key.variant < -1) continue;
            if (saved.contains("time") && !saved["time"].is_number()) continue;
            key.time = saved.value("time", 0.0f);
            key.amount = saved["amount"].get<float>();
            if (!std::isfinite(key.time) || !std::isfinite(key.amount)) continue;
            key.time = (std::max)(0.0f, key.time);
            key.amount = std::clamp(key.amount, -1.0f, 1.0f);
            if (saved.contains("right_hand") &&
                !saved["right_hand"].is_boolean()) continue;
            key.rightHand = saved.value("right_hand", false);
            keys.push_back(std::move(key));
        }
        animation.fistKeys = std::move(keys);
        return true;
    }

    inline bool LoadProfile(const std::string& name, AnimationComponent& animation,
        bool firstPerson = false)
    {
        const std::string path = ProfilePath(name);
        if (path.empty()) return false;
        try
        {
            std::ifstream input(path);
            if (!input) return false;
            json data;
            input >> data;
            if (!data.is_object()) return false;
            Read(data, animation, "punch_variants");
            ReadPoseKeys(data, animation,
                firstPerson ? "first_person_pose_keys" : "pose_keys");
            ReadFistKeys(data, animation,
                firstPerson ? "first_person_fist_keys" : "fist_keys");
            if (firstPerson)
                if (auto it = data.find("first_person_punch_clip");
                    it != data.end() && it->is_string())
                    for (auto& binding : animation.animations)
                        if (binding.state == AnimationState::Punch)
                        {
                            binding.name = it->get<std::string>();
                            binding.index = -1;
                            break;
                        }
            return true;
        }
        catch (const std::exception&) { return false; }
    }
}
