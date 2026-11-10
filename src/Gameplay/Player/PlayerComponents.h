#pragma once
#include <algorithm>

struct PlayerComponent
{
	float health = 100.0f;
	float stamina = 100.0f;
	float oxygen = 100.0f;
	
	float maxHealth = 100.0f;
	float maxStamina = 100.0f;
	float maxOxygen = 100.0f;

	

	void Normalize()
	{
		health = std::clamp(health, 0.0f, maxHealth);
		stamina = std::clamp(stamina, 0.0f, maxStamina);
		oxygen = std::clamp(oxygen, 0.0f, maxOxygen);
	}
};

