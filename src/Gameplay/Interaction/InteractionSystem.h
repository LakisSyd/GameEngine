#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <cameraclass.h>
#include "Gameplay/Player/GameplayInputSystem.h"
#include <ECSRegistry.h>
#include "../../../NPCComponents.h"
#include "EventSystem.h"
#include <PhysicsDebugRenderer.h>
#include <Physics/PhysicsRaycast.h>

struct InteractionTargetInfo
{
    Entity entity = INVALID_ENTITY;
    bool isNpc = false;
    bool isHostileNpc = false;
    float distance = 0.0f;
    std::string title;
    std::string description;
    std::string prompt;
};

class InteractionSystem
{
public:
    void Update(
        CameraClass* camera,
        const GameInputFrame& input,
        ECSRegistry* registry,
        btDynamicsWorld* world,
        PhysicsDebugRenderer* debugRenderer,
        const PhysicsDebugSettings& debugSettings,
        EventQueue& events)
    {
        m_hoveredEntity = INVALID_ENTITY;
        m_prompt.clear();
        m_hoveredTarget = {};

        if (!camera || !registry || !world)
            return;

        const bool interactionPressed = input.Pressed(GameAction::Interact);
        const bool escapePressed = input.Pressed(GameAction::CloseInteraction);
        bool closeInputConsumed = false;

        const auto* openedInteraction = m_openedEntity != INVALID_ENTITY
            ? registry->GetComponent<InteractionComponent>(m_openedEntity)
            : nullptr;
        if (m_openedEntity != INVALID_ENTITY &&
            (!registry->IsAlive(m_openedEntity) ||
                !registry->HasComponent<ActiveComponent>(m_openedEntity) ||
                (openedInteraction && !openedInteraction->enabled) ||
                (!openedInteraction &&
                    !registry->HasComponent<NPCComponent>(m_openedEntity))))
        {
            m_openedEntity = INVALID_ENTITY;
        }
        else if (m_openedEntity != INVALID_ENTITY &&
            (interactionPressed || escapePressed))
        {
            // Interact/Escape closes the current channel even after turning
            // away. Consume Interact so the same press cannot immediately interact
            // with another target behind the panel.
            m_openedEntity = INVALID_ENTITY;
            closeInputConsumed = interactionPressed;
        }

        Entity playerEntity = INVALID_ENTITY;
        const btCollisionObject* playerObject = nullptr;

        registry->ForEach<PlayerGameTag>(
            [&](Entity entity, PlayerGameTag&)
            {
                playerEntity = entity;

                if (auto* controller = registry->GetComponent<CharacterController>(entity))
                    playerObject = controller->ghostObj;
            });

        if (m_openedEntity != INVALID_ENTITY &&
            playerEntity != INVALID_ENTITY)
        {
            const auto* playerTransform =
                registry->GetComponent<TransformComponent>(playerEntity);
            const auto* openedTransform =
                registry->GetComponent<TransformComponent>(m_openedEntity);
            if (playerTransform && openedTransform)
            {
                const float dx = openedTransform->position.x - playerTransform->position.x;
                const float dy = openedTransform->position.y - playerTransform->position.y;
                const float dz = openedTransform->position.z - playerTransform->position.z;
                const float maxDistance = openedInteraction
                    ? (std::max)(0.0f, openedInteraction->maxDistance)
                    : m_defaultNpcInteractionDistance;
                const float closeDistance = maxDistance + 1.0f;
                if (dx * dx + dy * dy + dz * dz > closeDistance * closeDistance)
                    m_openedEntity = INVALID_ENTITY;
            }
        }

        PhysicsRaycastHit rayHit;
        if (!PhysicsRaycast::CastFromCamera(
                camera,
                world,
                1000.0f,
                playerEntity,
                playerObject,
                rayHit))
        {
            AddDebugRay(
                debugRenderer,
                debugSettings,
                rayHit.rayStart,
                rayHit.rayEnd,
                false);
            return;
        }

        AddDebugRay(
            debugRenderer,
            debugSettings,
            rayHit.rayStart,
            btVector3(rayHit.position.x, rayHit.position.y, rayHit.position.z),
            true);

        const Entity hitEntity = rayHit.entity;

        if (hitEntity == INVALID_ENTITY)
            return;

        const InteractionComponent* interaction =
            registry->GetComponent<InteractionComponent>(hitEntity);
        const NPCComponent* npc = registry->GetComponent<NPCComponent>(hitEntity);

        // NPCs are interactable by default. Adding a disabled InteractionComponent
        // explicitly opts an NPC out, while an enabled component can override its
        // prompt and range through the editor.
        if ((interaction && !interaction->enabled) || (!interaction && !npc))
            return;

        // Раньше maxDistance нигде не проверялся: любой объект на луче до 100 м
        // считался доступным. Теперь значение компонента действительно ограничивает доступ.
        const btVector3 hitPosition(rayHit.position.x, rayHit.position.y, rayHit.position.z);
        const float distanceSq = (hitPosition - rayHit.rayStart).length2();
        const float maxDistance = interaction
            ? (std::max)(0.0f, interaction->maxDistance)
            : m_defaultNpcInteractionDistance;
        if (distanceSq > maxDistance * maxDistance)
            return;

        m_hoveredEntity = hitEntity;
        m_hoveredTarget.entity = hitEntity;
        m_hoveredTarget.isNpc = npc != nullptr;
        m_hoveredTarget.isHostileNpc = npc && npc->isHostile;
        m_hoveredTarget.distance = std::sqrt(distanceSq);

        if (npc)
        {
            m_hoveredTarget.title = npc->name;
            m_hoveredTarget.description = npc->description;
        }
        else if (const auto* name = registry->GetComponent<NameComponent>(hitEntity))
        {
            m_hoveredTarget.title = name->name;
        }

        if (interaction && !interaction->prompt.empty())
            m_prompt = interaction->prompt;
        else if (npc && !npc->name.empty())
            m_prompt = "Talk to " + npc->name;
        else if (npc)
            m_prompt = "Talk";
        else
            m_prompt = "Interact";
        m_hoveredTarget.prompt = m_prompt;

        if (playerEntity != INVALID_ENTITY &&
            interactionPressed && !closeInputConsumed)
        {
            // Повторное действие Interact на том же объекте закрывает его окно.
            m_openedEntity = m_openedEntity == hitEntity
                ? INVALID_ENTITY
                : hitEntity;

            events.Emit(InteractionEvent{
                playerEntity,
                hitEntity
            });

            if (npc)
            {
                events.Emit(NpcInteractionEvent{
                    playerEntity,
                    hitEntity
                    });
            }
        }
    }

    Entity GetHoveredEntity() const
    {
        return m_hoveredEntity;
    }

    Entity GetOpenedEntity() const
    {
        return m_openedEntity;
    }

    const std::string& GetPrompt() const
    {
        return m_prompt;
    }

    const InteractionTargetInfo& GetHoveredTargetInfo() const
    {
        return m_hoveredTarget;
    }

    bool IsNpcHovered() const
    {
        return m_hoveredTarget.isNpc;
    }

    void CloseOpenedEntity()
    {
        m_openedEntity = INVALID_ENTITY;
    }

    void SetDefaultNpcInteractionDistance(float distance)
    {
        if (std::isfinite(distance))
            m_defaultNpcInteractionDistance = (std::max)(0.0f, distance);
    }

    float GetDefaultNpcInteractionDistance() const
    {
        return m_defaultNpcInteractionDistance;
    }

private:
    Entity m_hoveredEntity = INVALID_ENTITY;
    Entity m_openedEntity = INVALID_ENTITY;
    std::string m_prompt;
    InteractionTargetInfo m_hoveredTarget;
    float m_defaultNpcInteractionDistance = 3.0f;

    void AddDebugRay(
        PhysicsDebugRenderer* debugRenderer,
        const PhysicsDebugSettings& debugSettings,
        const btVector3& start,
        const btVector3& end,
        bool hit)
    {
        if (!debugRenderer || !debugSettings.showInteractionRays)
            return;

        debugRenderer->AddInteractionRay(
            XMFLOAT3(start.x(), start.y(), start.z()),
            XMFLOAT3(end.x(), end.y(), end.z()),
            hit,
            debugSettings.interactionRayLifetime);
    }
};
