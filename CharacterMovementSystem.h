#pragma once
#include "ECSRegistry.h"
#include "InputManager.h"
class ChatacterMovementSystem
{
public:

	static void Update(ECSRegistry& registry, float deltaTime, Entity PlayerEntity, InputClass* input)
	{
		/*if (!registry.HasComponent<PlayerGameTag>(PlayerEntity))
		{
			return;
		}*/

        auto* transform = registry.GetComponent<TransformComponent>(PlayerEntity);
        if (!transform)
            return;

        float speed = 5.0f;

        if (input->IsKeyDown('W'))
            transform->position.z += speed * deltaTime;

        if (input->IsKeyDown('S'))
            transform->position.z -= speed * deltaTime;

        if (input->IsKeyDown('A'))
            transform->position.x -= speed * deltaTime;

        if (input->IsKeyDown('D'))
            transform->position.x += speed * deltaTime;

		

	}

private:

};