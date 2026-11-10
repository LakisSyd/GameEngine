#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "ECSRegistry.h"
#include "EventSystem.h"

// DoorSystem отвечает за ФАКТИЧЕСКОЕ состояние и движение двери.
// NPC AI лишь вызывает DoorComponent::TryOpen(), когда подходит достаточно
// близко. После этого DoorSystem каждый кадр увеличивает openProgress, а NPC
// ждёт, пока проход откроется. Такое разделение позволяет игроку, NPC и скрипту
// использовать одну и ту же дверь без трёх разных реализаций анимации.
class DoorSystem
{
public:
    // Обработка обычного взаимодействия игрока/скрипта. Нажатие на створку тоже
    // находит корневой DoorComponent, чтобы объект было удобно настраивать в Blender.
    void ProcessEvents(ECSRegistry& registry, EventQueue& events)
    {
        std::vector<DoorOpenedEvent> openedDoors;

        for (const GameEvent& gameEvent : events.GetEvents())
        {
            const auto* interaction = std::get_if<InteractionEvent>(&gameEvent);
            if (!interaction)
                continue;

            Entity doorEntity = interaction->target;
            DoorComponent* door =
                registry.GetComponent<DoorComponent>(doorEntity);

            if (!door)
            {
                registry.ForEach<DoorComponent>(
                    [&](Entity candidateEntity, DoorComponent& candidateDoor)
                    {
                        if (door)
                            return;

                        if (candidateDoor.leftLeaf == interaction->target ||
                            candidateDoor.rightLeaf == interaction->target)
                        {
                            doorEntity = candidateEntity;
                            door = &candidateDoor;
                        }
                    });
            }

            if (!door || door->isBroken)
                continue;

            door->isOpen = !door->isOpen;

            if (door->isOpen)
            {
                openedDoors.push_back({
                    doorEntity,
                    interaction->interactor
                });
            }
        }

        // Emit only after iteration: EventQueue stores events in a vector.
        for (DoorOpenedEvent& event : openedDoors)
            events.Emit(std::move(event));
    }

    void Update(ECSRegistry& registry, float deltaTime)
    {
        registry.ForEach<DoorComponent>(
            [&](Entity entity, DoorComponent& door)
            {
                // isOpen — желаемое состояние, openProgress — фактическая фаза
                // анимации [0..1]. Сломанная дверь всегда движется к открытому виду.
                const float direction = (door.isOpen || door.isBroken) ? 1.0f : -1.0f;
                door.openProgress = std::clamp(
                    door.openProgress + direction * door.openSpeed * deltaTime,
                    0.0f,
                    1.0f);

                if (door.animationMode == DoorAnimationMode::Sliding &&
                    (door.leftLeaf != INVALID_ENTITY ||
                        door.rightLeaf != INVALID_ENTITY))
                {
                    TransformComponent* left = door.leftLeaf != INVALID_ENTITY
                        ? registry.GetComponent<TransformComponent>(door.leftLeaf)
                        : nullptr;
                    TransformComponent* right = door.rightLeaf != INVALID_ENTITY
                        ? registry.GetComponent<TransformComponent>(door.rightLeaf)
                        : nullptr;

                    if ((door.leftLeaf != INVALID_ENTITY && !left) ||
                        (door.rightLeaf != INVALID_ENTITY && !right))
                        return;

                    if (!door.leafPositionsInitialized)
                    {
                        // Закрытые позиции запоминаются один раз. В каждом кадре
                        // считаем положение именно от них, чтобы не копить float drift.
                        if (left)
                            door.leftClosedPosition = left->position;
                        if (right)
                            door.rightClosedPosition = right->position;

                        if (left && right)
                        {
                            const float dx = door.rightClosedPosition.x - door.leftClosedPosition.x;
                            const float dy = door.rightClosedPosition.y - door.leftClosedPosition.y;
                            const float dz = door.rightClosedPosition.z - door.leftClosedPosition.z;
                            const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
                            if (len > 0.0001f)
                            {
                                // По умолчанию ось проходит от левой створки к правой.
                                door.slideAxis = { dx / len, dy / len, dz / len };
                            }
                        }

                        door.leafPositionsInitialized = true;
                    }

                    // travel = 0 при закрытой двери и slideDistance при полностью
                    // открытой. openProgress даёт плавное движение между ними.
                    const float travel = door.slideDistance * door.openProgress;
                    if (left)
                    {
                        left->position = door.leftClosedPosition;
                        left->position.x -= door.slideAxis.x * travel;
                        left->position.y -= door.slideAxis.y * travel;
                        left->position.z -= door.slideAxis.z * travel;
                    }

                    if (right)
                    {
                        right->position = door.rightClosedPosition;
                        right->position.x += door.slideAxis.x * travel;
                        right->position.y += door.slideAxis.y * travel;
                        right->position.z += door.slideAxis.z * travel;
                    }
                    return;
                }

                // Старый/простой режим: корневая Entity двери поднимается по Y.
                TransformComponent* transform =
                    registry.GetComponent<TransformComponent>(entity);
                if (!transform)
                    return;

                if (!door.positionInitialized)
                {
                    door.closedPosition = transform->position;
                    door.positionInitialized = true;
                }

                transform->position = door.closedPosition;
                transform->position.y =
                    door.closedPosition.y + door.openHeight * door.openProgress;
            });
    }
};
