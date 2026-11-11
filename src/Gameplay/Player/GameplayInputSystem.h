#pragma once

#include <array>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <vector>

#include "Gameplay/Inventory/InventoryComponent.h"

class InputClass;
class CameraController;
class GameTimer;
class WeaponPickupSystem;
class ECSRegistry;
class CameraClass;

// Gameplay code asks for an action, never for a physical key or mouse button.
enum class GameAction : std::size_t
{
    MoveForward, MoveBackward, MoveLeft, MoveRight, Jump, Sprint,
    Fire, Aim, PrecisionAim, Reload, Interact, CloseInteraction, Repair,
    ToggleView, TogglePause, SlowerTime, FasterTime,
    DropWeapon, Slot1, Slot2, Slot3, ToggleDebugHud, ToggleEventLog,
    PrimaryMouse,
    Crouch,
    Count
};

enum class GameMouseButton { Left, Right, Middle };

struct GameInputBinding
{
    enum class Device { Keyboard, Mouse };
    Device device = Device::Keyboard;
    unsigned int key = 0;
    GameMouseButton button = GameMouseButton::Left;

    static GameInputBinding Key(unsigned int keyCode)
    {
        return { Device::Keyboard, keyCode, GameMouseButton::Left };
    }
    static GameInputBinding Mouse(GameMouseButton mouseButton)
    {
        return { Device::Mouse, 0, mouseButton };
    }
};

struct GameActionState
{
    bool down = false;
    bool pressed = false;
    bool released = false;
};

class GameInputFrame
{
public:
    const GameActionState& Get(GameAction action) const
    {
        return m_states[static_cast<std::size_t>(action)];
    }
    bool Down(GameAction action) const { return Get(action).down; }
    bool Pressed(GameAction action) const { return Get(action).pressed; }
    bool Released(GameAction action) const { return Get(action).released; }

private:
    friend class GameplayInputSystem;
    std::array<GameActionState, static_cast<std::size_t>(GameAction::Count)> m_states{};
};

class GameplayInputSystem
{
public:
    GameplayInputSystem();

    // Replace or add physical bindings without changing the consuming systems.
    void SetBindings(GameAction action, std::initializer_list<GameInputBinding> bindings);
    void AddBinding(GameAction action, GameInputBinding binding);
    const std::vector<GameInputBinding>& GetBindings(GameAction action) const;
    std::wstring GetBindingLabel(GameAction action) const;

    // Sample once per frame, after the Editor/Play and F2 decisions.
    // Locomotion may remain active while runtime UI owns the mouse; other
    // gameplay actions stay suppressed so clicking ImGui cannot fire a weapon.
    void Sample(InputClass* input, bool playMode, bool gameplayEnabled,
        bool locomotionEnabled = false);
    const GameInputFrame& GetFrame() const { return m_frame; }

    void UpdateSession(CameraController& camera, GameTimer& timer) const;
    bool UpdateInventory(WeaponPickupSystem& pickup, ECSRegistry& registry,
        Entity player, const CameraClass* camera) const;

private:
    static constexpr std::size_t ActionCount = static_cast<std::size_t>(GameAction::Count);
    std::array<std::vector<GameInputBinding>, ActionCount> m_bindings{};
    GameInputFrame m_frame{};
};
