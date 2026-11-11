#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <DirectXMath.h>

#include <Components.h>
#include <ECSRegistry.h>
#include "../../../NPCComponents.h"

#include "EventSystem.h"
#include "TriggerZoneComponent.h"

// Point-based, data-driven trigger evaluation. Player/NPC positions come from
// TransformComponent, so no Bullet ghost objects or collision callbacks are
// required. Runtime overlap/cooldown state deliberately lives here rather than
// in TriggerZoneComponent and is therefore reset on play/load as expected.
class TriggerZoneSystem
{
public:
    void Update(ECSRegistry& registry, float deltaTime, EventQueue& events)
    {
        if (std::isfinite(deltaTime) && deltaTime > 0.0f)
            m_timeSeconds += static_cast<double>(deltaTime);

        std::vector<ZoneCandidate> zones;
        registry.ForEach<TriggerZoneComponent, TransformComponent>(
            [&](Entity entity, TriggerZoneComponent& zone, TransformComponent& transform)
            {
                if (registry.IsAlive(entity))
                {
                    zones.push_back({
                        entity,
                        &zone,
                        &transform,
                        registry.HasComponent<ActiveComponent>(entity)
                        });
                }
            });
        std::sort(zones.begin(), zones.end(), EntityLess<ZoneCandidate>);

        std::vector<ActorCandidate> actors;
        registry.ForEach<TransformComponent>(
            [&](Entity entity, TransformComponent& transform)
            {
                if (!registry.IsAlive(entity))
                    return;

                TriggerZoneActorType type = TriggerZoneActorType::Other;
                if (!TryClassifyActor(registry, entity, type))
                    return;
                actors.push_back({ entity, type, transform.position });
            });
        std::sort(actors.begin(), actors.end(), EntityLess<ActorCandidate>);

        std::unordered_set<Entity> liveZones;
        liveZones.reserve(zones.size());

        for (const ZoneCandidate& candidate : zones)
        {
            liveZones.insert(candidate.entity);
            UpdateZone(
                registry,
                candidate.entity,
                *candidate.zone,
                *candidate.transform,
                candidate.entityActive,
                actors,
                events);
        }

        // Component/entity removal has no valid source left from which to emit
        // an Exit. Drop its private state so a reused/new entity cannot inherit it.
        for (auto it = m_runtime.begin(); it != m_runtime.end();)
        {
            if (!liveZones.contains(it->first))
                it = m_runtime.erase(it);
            else
                ++it;
        }
    }

    void Reset()
    {
        m_runtime.clear();
        m_timeSeconds = 0.0;
    }

    // Removes all overlap, cooldown, and one-shot state for one zone. This is
    // intended for scene resets/teleports and intentionally emits no events.
    void ResetZone(Entity zone)
    {
        m_runtime.erase(zone);
    }

    // Rearms a consumed one-shot zone. An actor already geometrically inside
    // must leave and enter again; this prevents an artificial Enter event.
    bool RearmZone(Entity zone, bool clearCooldowns = true)
    {
        const auto it = m_runtime.find(zone);
        if (it == m_runtime.end())
            return false;

        it->second.oneShotConsumed = false;
        if (clearCooldowns)
            it->second.lastActivationTime.clear();
        return true;
    }

    bool IsOneShotConsumed(Entity zone) const
    {
        const auto it = m_runtime.find(zone);
        return it != m_runtime.end() && it->second.oneShotConsumed;
    }

    // Geometric occupancy includes actors whose Enter was suppressed by a
    // cooldown or a consumed one-shot setting.
    bool IsActorInside(Entity zone, Entity actor) const
    {
        const auto zoneIt = m_runtime.find(zone);
        return zoneIt != m_runtime.end() &&
            zoneIt->second.geometricInside.contains(actor);
    }

    // Active occupants have received Enter and will receive a paired Exit.
    bool IsActorActivated(Entity zone, Entity actor) const
    {
        const auto zoneIt = m_runtime.find(zone);
        return zoneIt != m_runtime.end() &&
            zoneIt->second.activeOverlaps.contains(actor);
    }

    std::vector<Entity> GetActiveOccupants(Entity zone) const
    {
        std::vector<Entity> result;
        const auto zoneIt = m_runtime.find(zone);
        if (zoneIt == m_runtime.end())
            return result;

        result.reserve(zoneIt->second.activeOverlaps.size());
        for (const auto& [actor, overlap] : zoneIt->second.activeOverlaps)
        {
            (void)overlap;
            result.push_back(actor);
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    std::size_t GetActiveOccupantCount(Entity zone) const
    {
        const auto zoneIt = m_runtime.find(zone);
        return zoneIt == m_runtime.end()
            ? 0u
            : zoneIt->second.activeOverlaps.size();
    }

    double GetTimeSeconds() const noexcept { return m_timeSeconds; }

    static bool ContainsPoint(
        const TriggerZoneComponent& zone,
        const TransformComponent& transform,
        const DirectX::XMFLOAT3& worldPoint)
    {
        using namespace DirectX;

        const XMMATRIX world = transform.GetWorldMatrix();
        const XMVECTOR localCenter = XMLoadFloat3(&zone.localCenter);
        const XMVECTOR worldCenterVector = XMVector3TransformCoord(localCenter, world);
        XMFLOAT3 worldCenter;
        XMStoreFloat3(&worldCenter, worldCenterVector);

        if (zone.shape == TriggerZoneShape::Sphere)
        {
            const float scale = (std::max)({
                std::abs(transform.scale.x),
                std::abs(transform.scale.y),
                std::abs(transform.scale.z)
                });
            const float radius = std::abs(zone.sphereRadius) * scale;
            const float x = worldPoint.x - worldCenter.x;
            const float y = worldPoint.y - worldCenter.y;
            const float z = worldPoint.z - worldCenter.z;
            return x * x + y * y + z * z <= radius * radius;
        }

        XMVECTOR determinant;
        const XMMATRIX inverseWorld = XMMatrixInverse(&determinant, world);
        if (std::abs(XMVectorGetX(determinant)) <= 1.0e-8f)
            return false;

        const XMVECTOR localPointVector = XMVector3TransformCoord(
            XMLoadFloat3(&worldPoint),
            inverseWorld);
        XMFLOAT3 localPoint;
        XMStoreFloat3(&localPoint, localPointVector);

        const float x = localPoint.x - zone.localCenter.x;
        const float y = localPoint.y - zone.localCenter.y;
        const float z = localPoint.z - zone.localCenter.z;
        return std::abs(x) <= std::abs(zone.boxHalfExtents.x) &&
            std::abs(y) <= std::abs(zone.boxHalfExtents.y) &&
            std::abs(z) <= std::abs(zone.boxHalfExtents.z);
    }

private:
    struct ZoneCandidate
    {
        Entity entity = INVALID_ENTITY;
        TriggerZoneComponent* zone = nullptr;
        TransformComponent* transform = nullptr;
        bool entityActive = false;
    };

    struct ActorCandidate
    {
        Entity entity = INVALID_ENTITY;
        TriggerZoneActorType type = TriggerZoneActorType::Other;
        DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    };

    struct ActiveOverlap
    {
        TriggerZoneActorType actorType = TriggerZoneActorType::Other;
        double nextStayTime = 0.0;
    };

    struct ZoneRuntime
    {
        std::unordered_set<Entity> geometricInside;
        std::unordered_map<Entity, ActiveOverlap> activeOverlaps;
        std::unordered_map<Entity, double> lastActivationTime;
        bool oneShotConsumed = false;
    };

    std::unordered_map<Entity, ZoneRuntime> m_runtime;
    double m_timeSeconds = 0.0;

    template<typename T>
    static bool EntityLess(const T& left, const T& right)
    {
        return left.entity < right.entity;
    }

    static bool TryClassifyActor(
        ECSRegistry& registry,
        Entity entity,
        TriggerZoneActorType& type)
    {
        if (!registry.HasComponent<ActiveComponent>(entity))
            return false;
        if (registry.HasComponent<PlayerGameTag>(entity))
        {
            type = TriggerZoneActorType::Player;
            return true;
        }
        if (registry.HasComponent<NPCComponent>(entity))
        {
            type = TriggerZoneActorType::Npc;
            return true;
        }

        // "Other" means a movable world actor, not every Transform in the
        // scene. This prevents static meshes, editor anchors and neighbouring
        // trigger volumes from consuming one-shot zones before gameplay begins.
        const auto* rigidBody =
            registry.GetComponent<RigidBodyComponent>(entity);
        if (rigidBody && rigidBody->isDynamic)
        {
            type = TriggerZoneActorType::Other;
            return true;
        }
        return false;
    }

    static TriggerZoneActorMask MaskFor(TriggerZoneActorType type)
    {
        switch (type)
        {
        case TriggerZoneActorType::Player:
            return TriggerZoneActorMask::Player;
        case TriggerZoneActorType::Npc:
            return TriggerZoneActorMask::Npc;
        default:
            return TriggerZoneActorMask::Other;
        }
    }

    static void Emit(
        EventQueue& events,
        Entity zoneEntity,
        Entity actor,
        TriggerZoneActorType actorType,
        TriggerZoneEventType type,
        const TriggerZoneComponent& zone,
        double timeSeconds)
    {
        events.Emit(TriggerZoneEvent{
            zoneEntity,
            actor,
            zone.linkedEntity,
            type,
            actorType,
            zone.eventId,
            timeSeconds
            });
    }

    void UpdateZone(
        ECSRegistry& registry,
        Entity zoneEntity,
        const TriggerZoneComponent& zone,
        const TransformComponent& transform,
        bool entityActive,
        const std::vector<ActorCandidate>& actors,
        EventQueue& events)
    {
        ZoneRuntime& runtime = m_runtime[zoneEntity];

        for (auto it = runtime.lastActivationTime.begin();
            it != runtime.lastActivationTime.end();)
        {
            if (!registry.IsAlive(it->first))
                it = runtime.lastActivationTime.erase(it);
            else
                ++it;
        }

        if (!zone.enabled || !entityActive)
        {
            EmitAllExits(zoneEntity, zone, runtime, events);
            runtime.geometricInside.clear();
            return;
        }

        std::vector<ActorCandidate> inside;
        inside.reserve(actors.size());
        for (const ActorCandidate& actor : actors)
        {
            if (actor.entity == zoneEntity ||
                !HasTriggerActorFlag(zone.actorMask, MaskFor(actor.type)) ||
                !ContainsPoint(zone, transform, actor.position))
            {
                continue;
            }
            inside.push_back(actor);
        }

        std::unordered_set<Entity> currentInside;
        currentInside.reserve(inside.size());
        for (const ActorCandidate& actor : inside)
            currentInside.insert(actor.entity);

        // Exit is based on active overlaps rather than raw geometry. This keeps
        // Enter/Exit paired even if filter settings are changed at runtime.
        std::vector<Entity> exiting;
        for (const auto& [actor, overlap] : runtime.activeOverlaps)
        {
            (void)overlap;
            if (!currentInside.contains(actor))
                exiting.push_back(actor);
        }
        std::sort(exiting.begin(), exiting.end());
        for (Entity actor : exiting)
        {
            const auto overlap = runtime.activeOverlaps.find(actor);
            if (overlap == runtime.activeOverlaps.end())
                continue;
            Emit(
                events,
                zoneEntity,
                actor,
                overlap->second.actorType,
                TriggerZoneEventType::Exit,
                zone,
                m_timeSeconds);
            runtime.activeOverlaps.erase(overlap);
        }

        std::unordered_set<Entity> enteredThisUpdate;
        for (const ActorCandidate& actor : inside)
        {
            if (runtime.geometricInside.contains(actor.entity))
                continue;
            if (zone.oneShot && runtime.oneShotConsumed)
                continue;

            const float cooldown = (std::max)(0.0f, zone.cooldownSeconds);
            const auto lastActivation = runtime.lastActivationTime.find(actor.entity);
            if (lastActivation != runtime.lastActivationTime.end() &&
                m_timeSeconds - lastActivation->second < static_cast<double>(cooldown))
            {
                continue;
            }

            const double stayInterval = static_cast<double>(
                (std::max)(0.0f, zone.stayIntervalSeconds));
            runtime.activeOverlaps[actor.entity] = {
                actor.type,
                m_timeSeconds + stayInterval
            };
            runtime.lastActivationTime[actor.entity] = m_timeSeconds;
            enteredThisUpdate.insert(actor.entity);
            Emit(
                events,
                zoneEntity,
                actor.entity,
                actor.type,
                TriggerZoneEventType::Enter,
                zone,
                m_timeSeconds);

            if (zone.oneShot)
                runtime.oneShotConsumed = true;
        }

        if (zone.emitStay)
        {
            const double interval = static_cast<double>(
                (std::max)(0.0f, zone.stayIntervalSeconds));
            for (const ActorCandidate& actor : inside)
            {
                if (enteredThisUpdate.contains(actor.entity))
                    continue;
                auto overlap = runtime.activeOverlaps.find(actor.entity);
                if (overlap == runtime.activeOverlaps.end() ||
                    m_timeSeconds < overlap->second.nextStayTime)
                {
                    continue;
                }

                Emit(
                    events,
                    zoneEntity,
                    actor.entity,
                    overlap->second.actorType,
                    TriggerZoneEventType::Stay,
                    zone,
                    m_timeSeconds);
                overlap->second.nextStayTime = m_timeSeconds + interval;
            }
        }

        runtime.geometricInside = std::move(currentInside);
    }

    void EmitAllExits(
        Entity zoneEntity,
        const TriggerZoneComponent& zone,
        ZoneRuntime& runtime,
        EventQueue& events)
    {
        std::vector<Entity> occupants;
        occupants.reserve(runtime.activeOverlaps.size());
        for (const auto& [actor, overlap] : runtime.activeOverlaps)
        {
            (void)overlap;
            occupants.push_back(actor);
        }
        std::sort(occupants.begin(), occupants.end());

        for (Entity actor : occupants)
        {
            const auto overlap = runtime.activeOverlaps.find(actor);
            if (overlap == runtime.activeOverlaps.end())
                continue;
            Emit(
                events,
                zoneEntity,
                actor,
                overlap->second.actorType,
                TriggerZoneEventType::Exit,
                zone,
                m_timeSeconds);
        }
        runtime.activeOverlaps.clear();
    }
};
