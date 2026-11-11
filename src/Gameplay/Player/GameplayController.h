#pragma once

#include "EditorUI.h"
#include "CharacterMovementSystem.h"
#include "CharacterSpawnSystem.h"
#include "Gameplay/Combat/WeaponComponent.h"
#include "PlayerComponents.h"
#include "Gameplay/Inventory/InventoryComponent.h"
#include "DebugHud.h"
#include <windows.h>
#include "PlayerComponents.h"

enum class EngineMode
{
    Editor,
    Play
};


class GamePlayController
{
public:
    Entity GetPlayerEntity() const
    {
        return m_player;
    }

    void Update(
        ECSRegistry& registry,
        float dt,
        const GameInputFrame& input,
        bool gameplayBlocked,
        float cameraYaw)
    {
        if (gameplayBlocked)
        {
            // Если просто пропустить CharacterMovementSystem, в компоненте
            // останется направление предыдущего кадра и персонаж продолжит идти
            // сам, пока пользователь кликает debug UI. Поэтому явно гасим intent.
            if (auto* movement = registry.GetComponent<CharacterMovement>(m_player))
            {
                movement->walkDirection = { 0.0f, 0.0f, 0.0f };
                movement->currentSpeed = 0.0f;
                movement->wantsRun = false;
                movement->isRunning = false;
                movement->isMoving = false;
                movement->wantJump = false;
                movement->locomotionState = PlayerLocomotionState::Idle;
            }
            return;
        }


        CharacterMovementSystem::Update(registry, dt, input, cameraYaw);

        auto* player = registry.GetComponent<PlayerComponent>(m_player);

        if (!player)
            return;

        if (input.Down(GameAction::PrecisionAim))
        {
            player->aimMode = PlayerComponent::AimMode::Precise;

            DebugHud::Get().Show(
                "Precision Aim Mode",
                DebugHudMessageKind::Info,
                1.0f);
        }
        else if (input.Down(GameAction::Aim))
        {
            player->aimMode = PlayerComponent::AimMode::Shoulder;

            DebugHud::Get().Show(
                "Shoulder Aim Mode",
                DebugHudMessageKind::Info,
                1.0f);
        }
        else
        {
            player->aimMode = PlayerComponent::AimMode::Hip;
        }

        // Future gameplay systems:
        // InteractionSystem::Update(...);
        // MissionDirector::Update(...);
        // SpellSystem::Update(...);
    }

    bool EnterPlayMode(ECSRegistry& registry)
    {
        m_player = INVALID_ENTITY;

        Entity player = INVALID_ENTITY;
        int playerCount = 0;

        registry.ForEach<PlayerGameTag>([&](Entity entity, PlayerGameTag&)
        {
            player = entity;
            ++playerCount;
        });

        if (player == INVALID_ENTITY)
        {
            // A player is optional for AI test scenes.  Keep m_player invalid:
            // player-only systems already use this value as their guard, while
            // world physics and the NPC pipeline may continue in observer mode.
            OutputDebugStringA(
                "[Gameplay] No PlayerGameTag found; entering observer simulation mode.\n");
            return true;
        }

        if (playerCount > 1)
        {
            OutputDebugStringA("[Gameplay] Warning: multiple PlayerGameTag entities found; using the last one.\n");
        }

        auto* transform = registry.GetComponent<TransformComponent>(player);
        if (!transform)
        {
            OutputDebugStringA("[Gameplay] EnterPlayMode failed: player has no TransformComponent.\n");
            return false;
        }

		auto* anim = registry.GetComponent<AnimationComponent>(player);
        if(!anim)
        {
            registry.AddComponent<AnimationComponent>(player, AnimationComponent{});
            anim = registry.GetComponent<AnimationComponent>(player);
            OutputDebugStringA("[Gameplay] Added AnimationComponent to player.\n");
		}

		auto* playerComp = registry.GetComponent<PlayerComponent>(player);
        if (!playerComp)
        {
            registry.AddComponent<PlayerComponent>(player, PlayerComponent{});
            playerComp = registry.GetComponent<PlayerComponent>(player);
            OutputDebugStringA("[Gameplay] Added PlayerComponent to player.\n");
        }
        if (!registry.HasComponent<CharacterVitalsComponent>(player))
        {
            registry.AddComponent<CharacterVitalsComponent>(
                player, CharacterVitalsComponent{});
            OutputDebugStringA("[Gameplay] Added CharacterVitalsComponent to player.\n");
        }
        if (playerComp)
        {
            // При повторном запуске Play не продолжаем незаконченный удар.
            playerComp->punchActive = false;
            playerComp->punchHitApplied = false;
            playerComp->punchConsumedPrimary = false;
            playerComp->punchTime = 0.0f;
            playerComp->punchDuration = 0.0f;
            playerComp->punchLastHit = INVALID_ENTITY;
        }
        auto* movement = registry.GetComponent<CharacterMovement>(player);
        if (!movement)
        {
            registry.AddComponent<CharacterMovement>(player, CharacterMovement{});
            movement = registry.GetComponent<CharacterMovement>(player);
            OutputDebugStringA("[Gameplay] Added CharacterMovement to player.\n");
        }

        if (!movement)
        {
            OutputDebugStringA("[Gameplay] EnterPlayMode failed: CharacterMovement was not created.\n");
            return false;
        }

        auto* controller = registry.GetComponent<CharacterController>(player);
        if (!controller)
        {
            registry.AddComponent<CharacterController>(player, CharacterController{});
            controller = registry.GetComponent<CharacterController>(player);
            OutputDebugStringA("[Gameplay] Added CharacterController to player.\n");
        }

        if (!controller)
        {
            OutputDebugStringA("[Gameplay] EnterPlayMode failed: CharacterController was not created.\n");
            return false;
        }

        EnsureEquippedWeapon(registry, player);

        const bool controllerReady =
            controller->controller != nullptr &&
            controller->ghostObj != nullptr &&
            controller->capsuleShape != nullptr;

        const bool controllerEmpty =
            controller->controller == nullptr &&
            controller->ghostObj == nullptr &&
            controller->capsuleShape == nullptr;

        if (!controllerReady)
        {
            if (!controllerEmpty)
            {
                OutputDebugStringA("[Gameplay] EnterPlayMode failed: CharacterController is partially initialized.\n");
                return false;
            }

            CharacterSpawnSystem::Spawn(registry, player, transform->position);
            OutputDebugStringA("[Gameplay] Spawned Bullet character controller for player.\n");
        }

        m_player = player;
        return true;
    }

private:
    void EnsureEquippedWeapon(ECSRegistry& registry, Entity player)
    {
        if (player == INVALID_ENTITY)
            return;

        if (auto* equipped = registry.GetComponent<EquippedWeaponComponent>(player))
        {
            if (equipped->weapon != INVALID_ENTITY &&
                registry.HasComponent<WeaponComponent>(equipped->weapon))
            {
                return;
            }
        }

        // An existing inventory is authoritative even when its active slot is
        // empty. The player may have deliberately holstered or dropped the
        // last rifle; creating another starter weapon on every Play session
        // would duplicate items. InventorySystem repairs/synchronizes the
        // EquippedWeapon link immediately after EnterPlayMode.
        if (registry.HasComponent<InventoryComponent>(player))
            return;

        WeaponComponent weaponTemplate{};
        const bool hadLegacyWeapon = registry.HasComponent<WeaponComponent>(player);
        if (auto* legacyWeapon = registry.GetComponent<WeaponComponent>(player))
            weaponTemplate = *legacyWeapon;

        const Entity weaponEntity = registry.CreateEntity();
        registry.AddComponent<WeaponComponent>(weaponEntity, weaponTemplate);
        registry.AddComponent<WeaponOwnerComponent>(weaponEntity, WeaponOwnerComponent{ player });
        registry.AddComponent<ActiveComponent>(weaponEntity, ActiveComponent{});

        if (registry.HasComponent<EquippedWeaponComponent>(player))
            registry.RemoveComponent<EquippedWeaponComponent>(player);
        registry.AddComponent<EquippedWeaponComponent>(player, EquippedWeaponComponent{ weaponEntity });

        if (hadLegacyWeapon)
        {
            registry.RemoveComponent<WeaponComponent>(player);
            OutputDebugStringA("[Gameplay] Migrated player WeaponComponent to equipped weapon entity.\n");
        }
        else
        {
            OutputDebugStringA("[Gameplay] Created default equipped weapon entity for player.\n");
        }
    }

    Entity m_player = INVALID_ENTITY;


};
