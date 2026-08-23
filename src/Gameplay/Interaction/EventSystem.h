#pragma once

#include <utility>
#include <variant>
#include <vector>

#include <Components.h>

struct DoorOpenedEvent
{
    Entity door = INVALID_ENTITY;
    Entity opener = INVALID_ENTITY;
};

using GameEvent = std::variant<
    InteractionEvent,
    DoorOpenedEvent
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
