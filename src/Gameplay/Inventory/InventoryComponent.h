#pragma once

#include <array>
#include <cstddef>

#include <ECSTypes.h>

// An inventory stores references to gameplay item entities. The entities keep
// their own WeaponComponent, name and future item-specific state, so switching
// slots never copies or resets ammunition/cooldowns.
struct InventorySlot
{
    Entity item = INVALID_ENTITY;

    bool IsEmpty() const
    {
        return item == INVALID_ENTITY;
    }
};

struct InventoryComponent
{
    static constexpr std::size_t SlotCount = 3;
    static constexpr std::size_t InvalidSlot = SlotCount;

    std::array<InventorySlot, SlotCount> slots{};
    std::size_t activeSlot = 0;

    bool IsValidSlot(std::size_t slot) const
    {
        return slot < slots.size();
    }

    Entity GetItem(std::size_t slot) const
    {
        return IsValidSlot(slot)
            ? slots[slot].item
            : INVALID_ENTITY;
    }

    Entity GetActiveItem() const
    {
        return GetItem(activeSlot);
    }

    std::size_t FindItem(Entity item) const
    {
        if (item == INVALID_ENTITY)
            return InvalidSlot;

        for (std::size_t slot = 0; slot < slots.size(); ++slot)
        {
            if (slots[slot].item == item)
                return slot;
        }

        return InvalidSlot;
    }

    std::size_t FindFreeSlot() const
    {
        for (std::size_t slot = 0; slot < slots.size(); ++slot)
        {
            if (slots[slot].IsEmpty())
                return slot;
        }

        return InvalidSlot;
    }

    bool Contains(Entity item) const
    {
        return FindItem(item) != InvalidSlot;
    }

    std::size_t GetItemCount() const
    {
        std::size_t result = 0;

        for (const InventorySlot& slot : slots)
        {
            if (!slot.IsEmpty())
                ++result;
        }

        return result;
    }
};
