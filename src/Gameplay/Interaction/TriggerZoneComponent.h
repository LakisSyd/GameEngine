#pragma once

#include <cstdint>
#include <string>

#include <DirectXMath.h>

#include <ECSTypes.h>

// Authoring data for a trigger volume. A zone is an ordinary ECS entity with
// both TransformComponent and TriggerZoneComponent; it does not need a Bullet
// rigid body. This keeps gameplay triggers cheap and makes them safe to edit.
enum class TriggerZoneShape : std::uint8_t
{
    Box,
    Sphere
};

enum class TriggerZoneActorMask : std::uint32_t
{
    None = 0,
    Player = 1u << 0,
    Npc = 1u << 1,
    Other = 1u << 2,
    PlayerAndNpc = (1u << 0) | (1u << 1),
    All = (1u << 0) | (1u << 1) | (1u << 2)
};

constexpr TriggerZoneActorMask operator|(
    TriggerZoneActorMask left,
    TriggerZoneActorMask right) noexcept
{
    return static_cast<TriggerZoneActorMask>(
        static_cast<std::uint32_t>(left) |
        static_cast<std::uint32_t>(right));
}

constexpr TriggerZoneActorMask operator&(
    TriggerZoneActorMask left,
    TriggerZoneActorMask right) noexcept
{
    return static_cast<TriggerZoneActorMask>(
        static_cast<std::uint32_t>(left) &
        static_cast<std::uint32_t>(right));
}

constexpr bool HasTriggerActorFlag(
    TriggerZoneActorMask mask,
    TriggerZoneActorMask flag) noexcept
{
    return (mask & flag) != TriggerZoneActorMask::None;
}

enum class TriggerZoneEventType : std::uint8_t
{
    Enter,
    Stay,
    Exit
};

enum class TriggerZoneActorType : std::uint8_t
{
    Player,
    Npc,
    Other
};

struct TriggerZoneComponent
{
    bool enabled = true;
    TriggerZoneShape shape = TriggerZoneShape::Box;

    // Shape values are local to TransformComponent. Box rotation follows the
    // entity rotation and scale. A sphere always remains spherical and uses
    // the largest absolute transform scale component as its scale.
    DirectX::XMFLOAT3 localCenter = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 boxHalfExtents = { 1.0f, 1.0f, 1.0f };
    float sphereRadius = 1.0f;

    TriggerZoneActorMask actorMask = TriggerZoneActorMask::PlayerAndNpc;

    // oneShot consumes the zone after its first accepted Enter. The actor that
    // consumed it still receives Stay and Exit, so event lifecycles stay paired.
    bool oneShot = false;

    // Cooldown is per actor. An Enter rejected by cooldown stays rejected until
    // that actor leaves and crosses the boundary again.
    float cooldownSeconds = 0.0f;

    bool emitStay = true;
    // Zero emits one Stay per update. Positive values rate-limit Stay events.
    float stayIntervalSeconds = 0.0f;

    // Stable data-driven route for scripts/quests/HUD plus an optional entity
    // link. The trigger system only transports these values; consumers decide
    // what the event means.
    std::string eventId = "Trigger";
    Entity linkedEntity = INVALID_ENTITY;

    // Editor/debug renderer hints. They have no runtime physics cost.
    bool showDebugShape = true;
    DirectX::XMFLOAT4 debugColor = { 0.12f, 0.75f, 1.0f, 0.35f };
};

struct TriggerZoneEvent
{
    Entity zone = INVALID_ENTITY;
    Entity actor = INVALID_ENTITY;
    Entity linkedEntity = INVALID_ENTITY;
    TriggerZoneEventType type = TriggerZoneEventType::Enter;
    TriggerZoneActorType actorType = TriggerZoneActorType::Other;
    std::string eventId;
    double timeSeconds = 0.0;
};
