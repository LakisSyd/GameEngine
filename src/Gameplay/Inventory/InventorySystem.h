#pragma once

#include <cstddef>
#include <limits>

#include <ECSRegistry.h>

#include "Gameplay/Combat/WeaponComponent.h"
#include "InventoryComponent.h"

enum class InventoryStatus
{
    Success,
    InvalidOwner,
    MissingInventory,
    InvalidItem,
    ItemIsNotWeapon,
    OwnedByAnotherEntity,
    InventoryFull,
    AlreadyStored,
    InvalidSlot,
    EmptySlot,
    ItemNotStored
};

struct InventoryOperation
{
    InventoryStatus status = InventoryStatus::Success;
    Entity item = INVALID_ENTITY;
    std::size_t slot = InventoryComponent::InvalidSlot;

    explicit operator bool() const
    {
        return status == InventoryStatus::Success;
    }
};

// Header-only for now: the engine can use the inventory without adding a new
// translation unit to an existing Visual Studio project. The system owns only
// inventory/equipment relationships. Spawning a pickup mesh or placing a
// dropped item in the world stays in the scene/gameplay integration layer.
class InventorySystem
{
public:
    static constexpr std::size_t AnySlot =
        (std::numeric_limits<std::size_t>::max)();

    // Creates the component when needed and adopts a valid weapon that was
    // equipped by older scenes/GameplayController into the first slot.
    static InventoryComponent* EnsureInventory(
        ECSRegistry& registry,
        Entity owner)
    {
        if (!IsValidOwner(registry, owner))
            return nullptr;

        if (!registry.HasComponent<InventoryComponent>(owner))
        {
            registry.AddComponent<InventoryComponent>(
                owner,
                InventoryComponent{});
        }

        auto* inventory =
            registry.GetComponent<InventoryComponent>(owner);

        if (!inventory)
            return nullptr;

        SanitizeSlots(registry, owner, *inventory, false);
        AdoptLegacyEquippedWeapon(registry, owner, *inventory);
        NormalizeSelectedIndex(*inventory);
        SyncEquippedWeapon(registry, owner, *inventory);
        return inventory;
    }

    static InventoryOperation PickupWeapon(
        ECSRegistry& registry,
        Entity owner,
        Entity weapon,
        std::size_t preferredSlot = AnySlot,
        bool equipOnPickup = true)
    {
        if (!IsValidOwner(registry, owner))
            return { InventoryStatus::InvalidOwner, weapon };

        if (weapon == INVALID_ENTITY || !registry.IsAlive(weapon))
            return { InventoryStatus::InvalidItem, weapon };

        if (!registry.HasComponent<WeaponComponent>(weapon))
            return { InventoryStatus::ItemIsNotWeapon, weapon };

        if (const auto* ownership =
            registry.GetComponent<WeaponOwnerComponent>(weapon))
        {
            if (ownership->owner != INVALID_ENTITY &&
                ownership->owner != owner)
            {
                return {
                    InventoryStatus::OwnedByAnotherEntity,
                    weapon
                };
            }
        }

        InventoryComponent* inventory =
            EnsureInventory(registry, owner);

        if (!inventory)
            return { InventoryStatus::MissingInventory, weapon };

        const std::size_t existingSlot =
            inventory->FindItem(weapon);

        if (existingSlot != InventoryComponent::InvalidSlot)
        {
            if (equipOnPickup)
                SelectSlot(registry, owner, existingSlot);

            return {
                InventoryStatus::AlreadyStored,
                weapon,
                existingSlot
            };
        }

        std::size_t destination = preferredSlot;

        if (destination == AnySlot)
        {
            if (inventory->IsValidSlot(inventory->activeSlot) &&
                inventory->slots[inventory->activeSlot].IsEmpty())
            {
                destination = inventory->activeSlot;
            }
            else
            {
                destination = inventory->FindFreeSlot();
            }
        }

        if (!inventory->IsValidSlot(destination))
        {
            return {
                preferredSlot == AnySlot
                    ? InventoryStatus::InventoryFull
                    : InventoryStatus::InvalidSlot,
                weapon,
                destination
            };
        }

        if (!inventory->slots[destination].IsEmpty())
        {
            return {
                InventoryStatus::InventoryFull,
                weapon,
                destination
            };
        }

        inventory->slots[destination].item = weapon;
        SetOwner(registry, weapon, owner);

        if (equipOnPickup || inventory->GetItemCount() == 1)
            inventory->activeSlot = destination;

        SyncEquippedWeapon(registry, owner, *inventory);

        return {
            InventoryStatus::Success,
            weapon,
            destination
        };
    }

    // Selecting an empty slot intentionally holsters the current weapon. The
    // EmptySlot result tells UI/audio code that no item was equipped.
    static InventoryOperation SelectSlot(
        ECSRegistry& registry,
        Entity owner,
        std::size_t slot)
    {
        if (!IsValidOwner(registry, owner))
            return { InventoryStatus::InvalidOwner };

        auto* inventory =
            registry.GetComponent<InventoryComponent>(owner);

        if (!inventory)
            return { InventoryStatus::MissingInventory };

        SanitizeSlots(registry, owner, *inventory, false);

        if (!inventory->IsValidSlot(slot))
            return { InventoryStatus::InvalidSlot, INVALID_ENTITY, slot };

        inventory->activeSlot = slot;
        SyncEquippedWeapon(registry, owner, *inventory);

        const Entity item = inventory->GetActiveItem();
        return {
            item == INVALID_ENTITY
                ? InventoryStatus::EmptySlot
                : InventoryStatus::Success,
            item,
            slot
        };
    }

    static InventoryOperation SelectNext(
        ECSRegistry& registry,
        Entity owner,
        bool skipEmptySlots = true)
    {
        return SelectRelative(
            registry,
            owner,
            1,
            skipEmptySlots);
    }

    static InventoryOperation SelectPrevious(
        ECSRegistry& registry,
        Entity owner,
        bool skipEmptySlots = true)
    {
        return SelectRelative(
            registry,
            owner,
            -1,
            skipEmptySlots);
    }

    // Returns the same item entity so the caller can give it a transform,
    // interaction prompt and rigid body without recreating weapon state.
    static InventoryOperation DropSlot(
        ECSRegistry& registry,
        Entity owner,
        std::size_t slot)
    {
        if (!IsValidOwner(registry, owner))
            return { InventoryStatus::InvalidOwner };

        auto* inventory =
            registry.GetComponent<InventoryComponent>(owner);

        if (!inventory)
            return { InventoryStatus::MissingInventory };

        SanitizeSlots(registry, owner, *inventory, false);

        if (!inventory->IsValidSlot(slot))
            return { InventoryStatus::InvalidSlot, INVALID_ENTITY, slot };

        const Entity item = inventory->slots[slot].item;

        if (item == INVALID_ENTITY)
            return { InventoryStatus::EmptySlot, INVALID_ENTITY, slot };

        inventory->slots[slot].item = INVALID_ENTITY;

        if (auto* ownership =
            registry.GetComponent<WeaponOwnerComponent>(item))
        {
            if (ownership->owner == owner)
                registry.RemoveComponent<WeaponOwnerComponent>(item);
        }

        if (inventory->activeSlot == slot)
            SelectFirstOccupiedSlot(*inventory);

        SyncEquippedWeapon(registry, owner, *inventory);
        return { InventoryStatus::Success, item, slot };
    }

    static InventoryOperation DropActive(
        ECSRegistry& registry,
        Entity owner)
    {
        const auto* inventory =
            registry.GetComponent<InventoryComponent>(owner);

        if (!inventory)
            return { InventoryStatus::MissingInventory };

        return DropSlot(registry, owner, inventory->activeSlot);
    }

    static InventoryOperation DropItem(
        ECSRegistry& registry,
        Entity owner,
        Entity item)
    {
        const auto* inventory =
            registry.GetComponent<InventoryComponent>(owner);

        if (!inventory)
            return { InventoryStatus::MissingInventory, item };

        const std::size_t slot = inventory->FindItem(item);
        if (slot == InventoryComponent::InvalidSlot)
            return { InventoryStatus::ItemNotStored, item };

        return DropSlot(registry, owner, slot);
    }

    static InventoryOperation SwapSlots(
        ECSRegistry& registry,
        Entity owner,
        std::size_t first,
        std::size_t second)
    {
        if (!IsValidOwner(registry, owner))
            return { InventoryStatus::InvalidOwner };

        auto* inventory =
            registry.GetComponent<InventoryComponent>(owner);

        if (!inventory)
            return { InventoryStatus::MissingInventory };

        if (!inventory->IsValidSlot(first) ||
            !inventory->IsValidSlot(second))
        {
            return { InventoryStatus::InvalidSlot };
        }

        const Entity temporary = inventory->slots[first].item;
        inventory->slots[first].item = inventory->slots[second].item;
        inventory->slots[second].item = temporary;

        if (inventory->activeSlot == first)
            inventory->activeSlot = second;
        else if (inventory->activeSlot == second)
            inventory->activeSlot = first;

        SyncEquippedWeapon(registry, owner, *inventory);
        return {
            InventoryStatus::Success,
            inventory->GetActiveItem(),
            inventory->activeSlot
        };
    }

    // Call after scene/entity deletions. Invalid and duplicate references are
    // removed and EquippedWeaponComponent is repaired in one pass.
    static void Sanitize(
        ECSRegistry& registry,
        Entity owner)
    {
        if (!IsValidOwner(registry, owner))
            return;

        auto* inventory =
            registry.GetComponent<InventoryComponent>(owner);

        if (!inventory)
            return;

        SanitizeSlots(registry, owner, *inventory, true);
    }

private:
    static bool IsValidOwner(
        const ECSRegistry& registry,
        Entity owner)
    {
        return owner != INVALID_ENTITY && registry.IsAlive(owner);
    }

    static void SetOwner(
        ECSRegistry& registry,
        Entity item,
        Entity owner)
    {
        if (auto* ownership =
            registry.GetComponent<WeaponOwnerComponent>(item))
        {
            ownership->owner = owner;
            return;
        }

        registry.AddComponent<WeaponOwnerComponent>(
            item,
            WeaponOwnerComponent{ owner });
    }

    static void AdoptLegacyEquippedWeapon(
        ECSRegistry& registry,
        Entity owner,
        InventoryComponent& inventory)
    {
        const auto* equipped =
            registry.GetComponent<EquippedWeaponComponent>(owner);

        if (!equipped ||
            equipped->weapon == INVALID_ENTITY ||
            !registry.IsAlive(equipped->weapon) ||
            !registry.HasComponent<WeaponComponent>(equipped->weapon) ||
            inventory.Contains(equipped->weapon))
        {
            return;
        }

        const std::size_t freeSlot = inventory.FindFreeSlot();
        if (freeSlot == InventoryComponent::InvalidSlot)
            return;

        inventory.slots[freeSlot].item = equipped->weapon;
        inventory.activeSlot = freeSlot;
        SetOwner(registry, equipped->weapon, owner);
    }

    static void NormalizeSelectedIndex(InventoryComponent& inventory)
    {
        if (!inventory.IsValidSlot(inventory.activeSlot))
            inventory.activeSlot = 0;
    }

    static void SelectFirstOccupiedSlot(InventoryComponent& inventory)
    {
        for (std::size_t slot = 0;
            slot < InventoryComponent::SlotCount;
            ++slot)
        {
            if (!inventory.slots[slot].IsEmpty())
            {
                inventory.activeSlot = slot;
                return;
            }
        }

        if (!inventory.IsValidSlot(inventory.activeSlot))
            inventory.activeSlot = 0;
    }

    static void SyncEquippedWeapon(
        ECSRegistry& registry,
        Entity owner,
        const InventoryComponent& inventory)
    {
        const Entity activeItem = inventory.GetActiveItem();

        if (auto* equipped =
            registry.GetComponent<EquippedWeaponComponent>(owner))
        {
            if (equipped->weapon != activeItem)
                if (auto* previous = registry.GetComponent<WeaponComponent>(equipped->weapon))
                    previous->triggerHeld = false;
            equipped->weapon = activeItem;
            return;
        }

        if (activeItem != INVALID_ENTITY)
        {
            registry.AddComponent<EquippedWeaponComponent>(
                owner,
                EquippedWeaponComponent{ activeItem });
        }
    }

    static void SanitizeSlots(
        ECSRegistry& registry,
        Entity owner,
        InventoryComponent& inventory,
        bool syncEquipment)
    {
        NormalizeSelectedIndex(inventory);
        const bool activeSlotWasOccupied =
            inventory.GetActiveItem() != INVALID_ENTITY;

        for (std::size_t slot = 0;
            slot < InventoryComponent::SlotCount;
            ++slot)
        {
            const Entity item = inventory.slots[slot].item;
            bool valid =
                item != INVALID_ENTITY &&
                registry.IsAlive(item) &&
                registry.HasComponent<WeaponComponent>(item);

            if (valid)
            {
                if (const auto* ownership =
                    registry.GetComponent<WeaponOwnerComponent>(item))
                {
                    valid =
                        ownership->owner == INVALID_ENTITY ||
                        ownership->owner == owner;
                }
            }

            for (std::size_t previous = 0;
                valid && previous < slot;
                ++previous)
            {
                if (inventory.slots[previous].item == item)
                    valid = false;
            }

            if (!valid)
                inventory.slots[slot].item = INVALID_ENTITY;
            else
                SetOwner(registry, item, owner);
        }

        // Preserve an intentionally selected empty slot (holstered weapon), but
        // recover automatically when the selected entity was destroyed.
        if (activeSlotWasOccupied &&
            inventory.GetActiveItem() == INVALID_ENTITY)
        {
            SelectFirstOccupiedSlot(inventory);
        }

        if (syncEquipment)
            SyncEquippedWeapon(registry, owner, inventory);
    }

    static InventoryOperation SelectRelative(
        ECSRegistry& registry,
        Entity owner,
        int direction,
        bool skipEmptySlots)
    {
        if (!IsValidOwner(registry, owner))
            return { InventoryStatus::InvalidOwner };

        auto* inventory =
            registry.GetComponent<InventoryComponent>(owner);

        if (!inventory)
            return { InventoryStatus::MissingInventory };

        SanitizeSlots(registry, owner, *inventory, false);

        const std::size_t count = InventoryComponent::SlotCount;
        const std::size_t current =
            inventory->IsValidSlot(inventory->activeSlot)
            ? inventory->activeSlot
            : 0;

        for (std::size_t offset = 1; offset <= count; ++offset)
        {
            const std::size_t candidate = direction >= 0
                ? (current + offset) % count
                : (current + count - (offset % count)) % count;

            if (!skipEmptySlots ||
                !inventory->slots[candidate].IsEmpty())
            {
                return SelectSlot(registry, owner, candidate);
            }
        }

        SyncEquippedWeapon(registry, owner, *inventory);
        return {
            InventoryStatus::EmptySlot,
            inventory->GetActiveItem(),
            inventory->activeSlot
        };
    }
};
