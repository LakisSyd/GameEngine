#include "GameplayInputSystem.h"

#include <windows.h>

#include "CameraController.h"
#include "InputClass.h"
#include "Gameplay/Inventory/WeaponPickupSystem.h"
#include "../../../GameTimer.h"
#include "cameraclass.h"

namespace
{
    bool IsSessionAction(GameAction action)
    {
        return action == GameAction::ToggleView ||
            action == GameAction::TogglePause ||
            action == GameAction::SlowerTime ||
            action == GameAction::FasterTime ||
            action == GameAction::ToggleDebugHud ||
            action == GameAction::ToggleEventLog;
    }

    bool IsLocomotionAction(GameAction action)
    {
        return action == GameAction::MoveForward ||
            action == GameAction::MoveBackward ||
            action == GameAction::MoveLeft ||
            action == GameAction::MoveRight ||
            action == GameAction::Jump ||
			action == GameAction::Sprint || action == GameAction::Crouch;
    }

    bool IsBindingDown(InputClass* input, const GameInputBinding& binding)
    {
        if (!input) return false;
        if (binding.device == GameInputBinding::Device::Keyboard)
            return input->IsKeyDown(binding.key);
        switch (binding.button)
        {
        case GameMouseButton::Left: return input->IsLeftButtonDown();
        case GameMouseButton::Right: return input->IsRightButtonDown();
        case GameMouseButton::Middle: return input->IsMiddleButtonDown();
        }
        return false;
    }
}

GameplayInputSystem::GameplayInputSystem()
{
    using B = GameInputBinding;
    SetBindings(GameAction::MoveForward, { B::Key('W') });
    SetBindings(GameAction::MoveBackward, { B::Key('S') });
    SetBindings(GameAction::MoveLeft, { B::Key('A') });
    SetBindings(GameAction::MoveRight, { B::Key('D') });
    SetBindings(GameAction::Jump, { B::Key(VK_SPACE) });
    SetBindings(GameAction::Sprint, {
        B::Key(VK_SHIFT), B::Key(VK_LSHIFT), B::Key(VK_RSHIFT) });
    SetBindings(GameAction::Fire, { B::Mouse(GameMouseButton::Left), B::Key('H') });
    SetBindings(GameAction::PrimaryMouse, { B::Mouse(GameMouseButton::Left) });
    SetBindings(GameAction::Aim, { B::Mouse(GameMouseButton::Right) });
    SetBindings(GameAction::PrecisionAim, { B::Mouse(GameMouseButton::Middle) });
    SetBindings(GameAction::Reload, { B::Key('R') });
    SetBindings(GameAction::Interact, { B::Key('E') });
    SetBindings(GameAction::CloseInteraction, { B::Key(VK_ESCAPE) });
    SetBindings(GameAction::Repair, { B::Key('L') });
    SetBindings(GameAction::ToggleView, { B::Key('V') });
    SetBindings(GameAction::TogglePause, { B::Key('P') });
    SetBindings(GameAction::SlowerTime, { B::Key(VK_OEM_4) });
    SetBindings(GameAction::FasterTime, { B::Key(VK_OEM_6) });
    SetBindings(GameAction::DropWeapon, { B::Key('G') });
    SetBindings(GameAction::Slot1, { B::Key('1') });
    SetBindings(GameAction::Slot2, { B::Key('2') });
    SetBindings(GameAction::Slot3, { B::Key('3') });
    SetBindings(GameAction::ToggleDebugHud, { B::Key(VK_F3) });
    SetBindings(GameAction::ToggleEventLog, { B::Key(VK_TAB) });
	SetBindings(GameAction::Crouch, { B::Key('C') });
}

void GameplayInputSystem::SetBindings(GameAction action,
    std::initializer_list<GameInputBinding> bindings)
{
    m_bindings[static_cast<std::size_t>(action)] = bindings;
}

void GameplayInputSystem::AddBinding(GameAction action, GameInputBinding binding)
{
    m_bindings[static_cast<std::size_t>(action)].push_back(binding);
}

const std::vector<GameInputBinding>& GameplayInputSystem::GetBindings(GameAction action) const
{
    return m_bindings[static_cast<std::size_t>(action)];
}

std::wstring GameplayInputSystem::GetBindingLabel(GameAction action) const
{
    const auto& bindings = GetBindings(action);
    if (bindings.empty()) return L"";
    const auto& binding = bindings.front();
    if (binding.device == GameInputBinding::Device::Mouse)
    {
        switch (binding.button)
        {
        case GameMouseButton::Left: return L"LMB";
        case GameMouseButton::Right: return L"RMB";
        case GameMouseButton::Middle: return L"MMB";
        }
    }
    if ((binding.key >= 'A' && binding.key <= 'Z') ||
        (binding.key >= '0' && binding.key <= '9'))
        return std::wstring(1, static_cast<wchar_t>(binding.key));
    switch (binding.key)
    {
    case VK_SPACE: return L"SPACE";
    case VK_ESCAPE: return L"ESC";
    case VK_SHIFT: return L"SHIFT";
    case VK_TAB: return L"TAB";
    }
    return L"KEY";
}

void GameplayInputSystem::Sample(InputClass* input, bool playMode,
    bool gameplayEnabled, bool locomotionEnabled)
{
    for (std::size_t index = 0; index < ActionCount; ++index)
    {
        const auto action = static_cast<GameAction>(index);
        const bool enabled = playMode &&
            (gameplayEnabled || IsSessionAction(action) ||
                (locomotionEnabled && IsLocomotionAction(action)));
        bool down = false;
        if (enabled)
        {
            for (const auto& binding : m_bindings[index])
                down = down || IsBindingDown(input, binding);
        }
        auto& state = m_frame.m_states[index];
        state.pressed = down && !state.down;
        state.released = !down && state.down;
        state.down = down;
    }
}

void GameplayInputSystem::UpdateSession(CameraController& camera, GameTimer& timer) const
{
    if (m_frame.Pressed(GameAction::ToggleView))
        camera.SetPlayViewMode(camera.IsFirstPerson()
            ? CameraController::PlayViewMode::ThirdPerson
            : CameraController::PlayViewMode::FirstPerson);
    if (m_frame.Pressed(GameAction::TogglePause))
        timer.SetPaused(!timer.IsPaused());
    if (m_frame.Pressed(GameAction::SlowerTime))
        timer.SetTimeScale(timer.GetTimeScale() - 0.25f);
    if (m_frame.Pressed(GameAction::FasterTime))
        timer.SetTimeScale(timer.GetTimeScale() + 0.25f);
}

bool GameplayInputSystem::UpdateInventory(WeaponPickupSystem& pickup,
    ECSRegistry& registry, Entity player, const CameraClass* camera) const
{
    static_assert(InventoryComponent::SlotCount == 3,
        "Update the slot action list when inventory size changes");
    constexpr std::array<GameAction, InventoryComponent::SlotCount> slotActions = {
        GameAction::Slot1, GameAction::Slot2, GameAction::Slot3
    };
    bool changed = false;
    for (std::size_t slot = 0; slot < slotActions.size(); ++slot)
    {
        if (!m_frame.Pressed(slotActions[slot])) continue;
        pickup.SelectSlot(registry, player, slot);
        changed = true;
    }
    if (camera && m_frame.Pressed(GameAction::DropWeapon))
    {
        pickup.DropActive(registry, player,
            camera->GetPosition(), camera->GetForward());
        changed = true;
    }
    return pickup.ConsumeInventoryChanged() || changed;
}
