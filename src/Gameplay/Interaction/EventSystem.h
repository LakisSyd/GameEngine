#pragma once

#include <utility>
#include <variant>
#include <vector>

#include <Components.h>

#include "TriggerZoneComponent.h"

struct DoorOpenedEvent
{
    Entity door = INVALID_ENTITY;
    Entity opener = INVALID_ENTITY;
};

// Emitted in addition to InteractionEvent when the target is an NPC. Dialogue,
// crew-command, reputation, and HUD systems can subscribe without duplicating
// the ECS type check performed by InteractionSystem.
struct NpcInteractionEvent
{
    Entity interactor = INVALID_ENTITY;
    Entity npc = INVALID_ENTITY;
};

using GameEvent = std::variant<
    InteractionEvent,
    DoorOpenedEvent,
    NpcInteractionEvent,
    TriggerZoneEvent
>;

class EventQueue
{
public:
    template<typename T>
    void Emit(T&& event)
    {
        m_events.emplace_back(std::forward<T>(event));
    }

    const std::vector<GameEvent>& GetEvents() const
    {
        return m_events;
    }

    void Clear()
    {
        m_events.clear();
    }

private:
    std::vector<GameEvent> m_events;
};
