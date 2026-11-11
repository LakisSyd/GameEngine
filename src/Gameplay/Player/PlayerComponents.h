#pragma once
#include <DirectXMath.h>
#include "ECSTypes.h"
#include "CharacterVitalsComponent.h"

struct PlayerComponent
{
 enum class AimMode
	{
		Hip,
		Shoulder,
		Precise
	};

	AimMode aimMode = AimMode::Hip;

	// Настройки удара сохраняются вместе с игроком и доступны в Punch Studio.
	float punchDamage = 10.0f;
	float punchReach = 1.25f;
	float punchContactRadius = 0.25f;
	float punchContactPhase = 0.50f;
	float punchIKStrength = 1.0f;
	float punchFirstPersonIKStrength = 0.0f;
	DirectX::XMFLOAT3 punchFistOffset = { 0.0f, 0.0f, 0.0f };

	// Эти поля живут только во время игры: один клик запускает один удар.
	bool punchActive = false;
	bool meleeGuardActive = false;
	bool punchHitApplied = false;
	bool punchConsumedPrimary = false;
	int punchVariant = 0;
	int punchNextVariant = 0;
	float punchTime = 0.0f;
	float punchDuration = 0.0f;
	DirectX::XMFLOAT3 punchAimOrigin = {};
	DirectX::XMFLOAT3 punchAimDirection = { 0.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT3 punchAimPoint = {};
	DirectX::XMFLOAT3 punchFistWorld = {};
	float punchAimError = 0.0f;
	Entity punchLastHit = INVALID_ENTITY;
};

