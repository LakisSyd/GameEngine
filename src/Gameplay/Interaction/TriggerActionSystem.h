#pragma once

#include <algorithm>
#include <string>
#include <variant>

#include <Components.h>
#include <ECSRegistry.h>

#include "EventSystem.h"

// Built-in actions make common trigger workflows usable entirely from the
// editor. eventId remains available for custom game code, but a level designer
// can already display a message, activate an entity, or control a door.
enum class TriggerActionType
{
    None,
    ShowHudMessage,
    SetEntityActive,
    ToggleEntityActive,
    SetDoorOpen
};

inline const char* TriggerActionTypeName(TriggerActionType type)
{
    switch (type)
    {
    case TriggerActionType::ShowHudMessage: return "Show HUD message";
    case TriggerActionType::SetEntityActive: return "Set entity active";
    case TriggerActionType::ToggleEntityActive: return "Toggle entity active";
    case TriggerActionType::SetDoorOpen: return "Set door open";
    default: return "Emit event only";
    }
}

struct TriggerActionComponent
{
    TriggerActionType action = TriggerActionType::ShowHudMessage;
    Entity target = INVALID_ENTITY;

    bool executeOnEnter = true;
    bool executeOnStay = false;
    bool executeOnExit = false;

    // Used by SetEntityActive and SetDoorOpen. When executeOnExit and
    // invertOnExit are enabled the action automatically restores the opposite
    // state, which is useful for lights, doors, and room-local systems.
    bool value = true;
    bool invertOnExit = true;

    std::string hudMessage = "Entered trigger zone";
    float messageDuration = 3.0f;
};

struct TriggerActionFeedback
{
    bool visible = false;
    std::string message;
    float remainingSeconds = 0.0f;
    Entity sourceZone = INVALID_ENTITY;
};

class TriggerActionSystem
{
public:
    void Update(
        ECSRegistry& registry,
        const EventQueue& events,
        float deltaTime,
        bool processEvents = true)
    {
        m_feedback.remainingSeconds = (std::max)(
            0.0f, m_feedback.remainingSeconds - (std::max)(0.0f, deltaTime));
        m_feedback.visible = m_feedback.remainingSeconds > 0.0f;
        m_activationsThisFrame = 0;

        if (!processEvents)
            return;

        for (const GameEvent& gameEvent : events.GetEvents())
        {
            const auto* trigger = std::get_if<TriggerZoneEvent>(&gameEvent);
            if (!trigger)
                continue;

            const auto* action =
                registry.GetComponent<TriggerActionComponent>(trigger->zone);
            if (!action || !ShouldExecute(*action, trigger->type))
                continue;

            Execute(registry, *trigger, *action);
            ++m_activationsThisFrame;
        }
    }

    const TriggerActionFeedback& GetFeedback() const { return m_feedback; }
    int GetActivationsThisFrame() const { return m_activationsThisFrame; }

private:
    TriggerActionFeedback m_feedback;
    int m_activationsThisFrame = 0;

    static bool ShouldExecute(
        const TriggerActionComponent& action,
        TriggerZoneEventType eventType)
    {
        switch (eventType)
        {
        case TriggerZoneEventType::Enter: return action.executeOnEnter;
        case TriggerZoneEventType::Stay: return action.executeOnStay;
        case TriggerZoneEventType::Exit: return action.executeOnExit;
        default: return false;
        }
    }

    void Execute(
        ECSRegistry& registry,
        const TriggerZoneEvent& event,
        const TriggerActionComponent& action)
    {
        const Entity target = action.target != INVALID_ENTITY
            ? action.target
            : event.linkedEntity;
        const bool value =
            event.type == TriggerZoneEventType::Exit && action.invertOnExit
            ? !action.value
            : action.value;

        switch (action.action)
        {
        case TriggerActionType::ShowHudMessage:
            m_feedback.visible = true;
            m_feedback.message = action.hudMessage.empty()
                ? event.eventId
                : action.hudMessage;
            m_feedback.remainingSeconds = (std::max)(
                0.1f, action.messageDuration);
            m_feedback.sourceZone = event.zone;
            break;

        case TriggerActionType::SetEntityActive:
            if (target != INVALID_ENTITY && registry.IsAlive(target))
            {
                if (value && !registry.HasComponent<ActiveComponent>(target))
                    registry.AddComponent<ActiveComponent>(target, ActiveComponent{});
                else if (!value)
                    registry.RemoveComponent<ActiveComponent>(target);
            }
            break;

        case TriggerActionType::ToggleEntityActive:
            if (target != INVALID_ENTITY && registry.IsAlive(target))
            {
                if (registry.HasComponent<ActiveComponent>(target))
                    registry.RemoveComponent<ActiveComponent>(target);
                else
                    registry.AddComponent<ActiveComponent>(target, ActiveComponent{});
            }
            break;

        case TriggerActionType::SetDoorOpen:
            if (auto* door = registry.GetComponent<DoorComponent>(target))
                door->isOpen = value;
            break;

        default:
            break;
        }
    }
};
