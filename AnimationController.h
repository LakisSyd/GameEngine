#pragma once
#include "ECSRegistry.h"
class AnimationController
{
public:
	static void Update(ECSRegistry& registry, float deltaTime)
	{
		Entity player = INVALID_ENTITY;
		registry.ForEach<AnimationComponent>([&](Entity entity, AnimationComponent& anim)
			{
				player = entity;
			});

		auto* movement = registry.GetComponent<CharacterMovement>(player);
		auto anim = registry.GetComponent<AnimationComponent>(player);
		
		if(!movement || !anim)
			return;

		if (movement->currentSpeed > 0.1f)
		{
			anim->isPlaying = true;
			anim->animationIndex = 2; // walk/run
		}
		else
		{
			anim->isPlaying = true;
			anim->animationIndex = 1; // idle
		}
	}
};