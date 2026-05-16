#pragma once

#include "EditorUI.h"
#include "../../CharacterMovementSystem.h"


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
            bool gameplayBlocked)
        {
            if (gameplayBlocked)
                return;

            

            CharacterMovementSystem::Update(registry, dt, input);

            // Потом сюда добавишь:
            // InteractionSystem::Update(...);
            // MissionDirector::Update(...);
            // SpellSystem::Update(...);
        }

		


};