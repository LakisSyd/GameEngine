#pragma once
#include <algorithm>
#include <cameraclass.h>
#include <InputManager.h>
#include <InputClass.h>
#include <ECSRegistry.h>
#include "EventSystem.h"
#include <PhysicsDebugRenderer.h>
#include <Physics/PhysicsRaycast.h>

class InteractionSystem
{
public:
    void Update(
        CameraClass* camera,
        InputManager* input,
        ECSRegistry* registry,
        btDynamicsWorld* world,
        InputClass* input_m,
        PhysicsDebugRenderer* debugRenderer,
        const PhysicsDebugSettings& debugSettings,
        EventQueue& events)
    {
        m_hoveredEntity = INVALID_ENTITY;
        m_prompt.clear();

        if (!camera || !input || !registry || !world)
            return;

        Entity playerEntity = INVALID_ENTITY;
        const btCollisionObject* playerObject = nullptr;

        registry->ForEach<PlayerGameTag>(
            [&](Entity entity, PlayerGameTag&)
            {
                playerEntity = entity;

                if (auto* controller = registry->GetComponent<CharacterController>(entity))
                    playerObject = controller->ghostObj;
            });

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

        InteractionComponent* interaction =
            registry->GetComponent<InteractionComponent>(hitEntity);

        if (!interaction || !interaction->enabled)
            return;

        // Раньше maxDistance нигде не проверялся: любой объект на луче до 100 м
        // считался доступным. Теперь значение компонента действительно ограничивает доступ.
        const btVector3 hitPosition(rayHit.position.x, rayHit.position.y, rayHit.position.z);
        const float distanceSq = (hitPosition - rayHit.rayStart).length2();
        const float maxDistance = (std::max)(0.0f, interaction->maxDistance);
        if (distanceSq > maxDistance * maxDistance)
            return;

        m_hoveredEntity = hitEntity;
        m_prompt = interaction->prompt;

        if (playerEntity != INVALID_ENTITY &&
            input->IsKeyPressed(input_m, 'E'))
        {
            // Повторное E на том же объекте закрывает его окно.
            m_openedEntity = m_openedEntity == hitEntity
                ? INVALID_ENTITY
                : hitEntity;

            events.Emit(InteractionEvent{
                playerEntity,
                hitEntity
            });
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

private:
    Entity m_hoveredEntity = INVALID_ENTITY;
    Entity m_openedEntity = INVALID_ENTITY;
    std::string m_prompt;

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
