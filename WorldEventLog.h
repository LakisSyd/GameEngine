#pragma once

#include <deque>
#include <cstddef>

#include "WorldEvent.h"

class WorldEventLog
{
public:
    explicit WorldEventLog(size_t maxEvents = 512)
        : m_maxEvents(maxEvents)
    {
    }

    void Push(const WorldEvent& event)
    {
        if (m_events.size() >= m_maxEvents)
            m_events.pop_front();

        m_events.push_back(event);
    }

    const std::deque<WorldEvent>& GetEvents() const
    {
        return m_events;
    }

    void Clear()
    {
        m_events.clear();
    }

    size_t Size() const
    {
        return m_events.size();
    }

private:
    std::deque<WorldEvent> m_events;
    size_t m_maxEvents = 512;
};