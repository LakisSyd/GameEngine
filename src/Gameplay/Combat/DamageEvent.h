#pragma once
#include <cstdint>
#include <ECSTypes.h>
#include <DirectXMath.h>

struct DamageEvent
{
	enum class DamageType
	{
		Physical,
		Fire,
		Ice,
		Electric,
		Poison,
		Radiation,
		Psychic,
		Sonic,
		Acid,
		Laser,
		Nuclear,
		
	};

	Entity target = INVALID_ENTITY;
	Entity source = INVALID_ENTITY;

	float damageAmount = 0.f;

	DirectX::XMFLOAT3 hit_position = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 hit_normal = { 0.0f, 0.0f, 0.0f };

	DamageType damageType = DamageType::Physical;

	// Будущее тихое убийство выставит true. Урон пройдёт как
	// обычно, но жертва не получит underFire/направление на атаку.
	bool silentKill = false;

	// Hull damage capability and actual penetration are separate. The weapon may
	// damage armor now, while the panel decides whether enough damage accumulated.
	bool canDamageHull = false;
	bool canBreachHull = false;
	float breachRadius = 0.0f;
	std::uint64_t meshCutOperationId = 0;

};
