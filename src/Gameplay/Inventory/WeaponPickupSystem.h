#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <variant>

#include <DirectXMath.h>

#include <Components.h>
#include <ECSRegistry.h>
#include <PhysicsSystem.h>

#include "Gameplay/Interaction/EventSystem.h"
#include "InventorySystem.h"

// Authoring component for a world weapon. WeaponComponent describes how the
// rifle fires; this component describes how it enters and leaves an inventory.
struct WeaponPickupComponent
{
    bool enabled = true;
    bool equipOnPickup = true;
    bool createPhysicsOnDrop = true;
    float dropMass = 1.5f;
    DirectX::XMFLOAT3 dropCollisionSize = { 0.35f, 0.18f, 0.90f };
    std::string prompt = "Pick up rifle";
};

struct InventoryFeedback
{
    bool visible = false;
    bool success = false;
    std::string message;
    float remainingSeconds = 0.0f;
};

class WeaponPickupSystem
{
public:
    void Update(
        ECSRegistry& registry,
        Entity player,
        const EventQueue& events,
        float deltaTime,
        bool processInteractions = true)
    {
        m_feedback.remainingSeconds = (std::max)(
            0.0f, m_feedback.remainingSeconds - (std::max)(0.0f, deltaTime));
        m_feedback.visible = m_feedback.remainingSeconds > 0.0f;

        if (!processInteractions)
            return;

        if (player == INVALID_ENTITY || !registry.IsAlive(player))
            return;

        for (const GameEvent& gameEvent : events.GetEvents())
        {
            const auto* interaction = std::get_if<InteractionEvent>(&gameEvent);
            if (!interaction || interaction->interactor != player)
                continue;

            auto* pickup =
                registry.GetComponent<WeaponPickupComponent>(interaction->target);
            if (!pickup || !pickup->enabled)
                continue;

            const InventoryOperation operation = InventorySystem::PickupWeapon(
                registry,
                player,
                interaction->target,
                InventorySystem::AnySlot,
                pickup->equipOnPickup);

            if (!operation)
            {
                SetFeedback(false, StatusMessage(operation.status));
                continue;
            }

            pickup->enabled = false;
            registry.RemoveComponent<InteractionComponent>(interaction->target);
            registry.RemoveComponent<RigidBodyComponent>(interaction->target);
            registry.RemoveComponent<ActiveComponent>(interaction->target);

            SetFeedback(
                true,
                "Rifle stored in slot " + std::to_string(operation.slot + 1));
            m_inventoryChanged = true;
        }
    }

    InventoryOperation SelectSlot(
        ECSRegistry& registry,
        Entity player,
        std::size_t slot)
    {
        const InventoryOperation result =
            InventorySystem::SelectSlot(registry, player, slot);
        if (result || result.status == InventoryStatus::EmptySlot)
        {
            SetFeedback(
                true,
                result.item != INVALID_ENTITY
                    ? "Equipped slot " + std::to_string(slot + 1)
                    : "Slot " + std::to_string(slot + 1) + " is empty");
            m_inventoryChanged = true;
        }
        return result;
    }

    InventoryOperation DropActive(
        ECSRegistry& registry,
        Entity player,
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT3& forward)
    {
        const InventoryOperation result =
            InventorySystem::DropActive(registry, player);
        if (!result)
        {
            SetFeedback(false, StatusMessage(result.status));
            return result;
        }

        auto* pickup = registry.GetComponent<WeaponPickupComponent>(result.item);
        if (!pickup)
        {
            registry.AddComponent<WeaponPickupComponent>(
                result.item, WeaponPickupComponent{});
            pickup = registry.GetComponent<WeaponPickupComponent>(result.item);
        }

        TransformComponent* transform =
            registry.GetComponent<TransformComponent>(result.item);
        if (!transform)
        {
            registry.AddComponent<TransformComponent>(
                result.item, TransformComponent{});
            transform = registry.GetComponent<TransformComponent>(result.item);
        }
        if (transform)
        {
            transform->position = {
                position.x + forward.x * 1.25f,
                position.y + 0.35f,
                position.z + forward.z * 1.25f
            };
        }

        if (pickup)
        {
            pickup->enabled = true;
            InteractionComponent interaction;
            interaction.enabled = true;
            interaction.maxDistance = 3.0f;
            interaction.prompt = pickup->prompt;
            registry.AddComponent<InteractionComponent>(result.item, interaction);

            if (pickup->createPhysicsOnDrop &&
                !registry.HasComponent<RigidBodyComponent>(result.item))
            {
                RigidBodyComponent body;
                body.mass = (std::max)(0.01f, pickup->dropMass);
                body.isDynamic = true;
                body.shapeType = RigidBodyComponent::Box;
                body.useManualSize = true;
                body.shapeSize = btVector3(
                    (std::max)(0.05f, pickup->dropCollisionSize.x),
                    (std::max)(0.05f, pickup->dropCollisionSize.y),
                    (std::max)(0.05f, pickup->dropCollisionSize.z));
                registry.AddComponent<RigidBodyComponent>(result.item, std::move(body));
                PhysicsSystem::CreateRigidBody(registry, result.item);
            }
        }

        registry.AddComponent<ActiveComponent>(result.item, ActiveComponent{});
        SetFeedback(true, "Dropped rifle from slot " + std::to_string(result.slot + 1));
        m_inventoryChanged = true;
        return result;
    }

    const InventoryFeedback& GetFeedback() const { return m_feedback; }

    bool ConsumeInventoryChanged()
    {
        const bool changed = m_inventoryChanged;
        m_inventoryChanged = false;
        return changed;
    }

private:
    InventoryFeedback m_feedback;
    bool m_inventoryChanged = false;

    void SetFeedback(bool success, std::string message)
    {
        m_feedback.visible = true;
        m_feedback.success = success;
        m_feedback.message = std::move(message);
        m_feedback.remainingSeconds = 2.2f;
    }

    static const char* StatusMessage(InventoryStatus status)
    {
        switch (status)
        {
        case InventoryStatus::InventoryFull: return "Inventory is full";
        case InventoryStatus::EmptySlot: return "Selected slot is empty";
        case InventoryStatus::OwnedByAnotherEntity: return "Item belongs to another crew member";
        case InventoryStatus::ItemIsNotWeapon: return "This item cannot be stored as a weapon";
        case InventoryStatus::MissingInventory: return "Inventory is unavailable";
        case InventoryStatus::InvalidOwner: return "No valid inventory owner";
        default: return "Inventory action failed";
        }
    }
};
