#pragma once

#include "EditorUI.h"
#include "../../CharacterMovementSystem.h"
#include "../../CharacterSpawnSystem.h"

enum class EngineMode
{
    Editor,
    Play
};

class GamePlayController
{


	public:
       
        void Update(
            ECSRegistry& registry,
            float dt,
            InputClass* input,
            bool gameplayBlocked, float cameraYaw)
        {
            if (gameplayBlocked)
                return;

            

            CharacterMovementSystem::Update(registry, dt, input,cameraYaw);

            

            // Потом сюда добавишь:
            // InteractionSystem::Update(...);
            // MissionDirector::Update(...);
            // SpellSystem::Update(...);
        }

		bool EnterPlayerMode(ECSRegistry& registry)
        {
           
			Entity player = INVALID_ENTITY;

            registry.ForEach<PlayerGameTag>([&](Entity entity, PlayerGameTag& tag)
                {
                    player = entity;
                });

            auto* controller = registry.GetComponent<CharacterController>(player);
			auto* movement = registry.GetComponent<CharacterMovement>(player);
            auto* transform = registry.GetComponent<TransformComponent>(player);

            if (!controller)
            {
                registry.AddComponent<CharacterController>(player, CharacterController{});
              
                controller = registry.GetComponent<CharacterController>(player);
            }
            if(!movement)
            {
                registry.AddComponent<CharacterMovement>(player, CharacterMovement{});
			}
            if (!controller->controller)
            {
                CharacterSpawnSystem::Spawn(registry, player, transform->position);
            }

            return true; 
		}


};