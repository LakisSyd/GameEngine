#pragma once

#include <ECSTypes.h>
#include <DirectXMath.h>
#include <string>

enum class WeaponType
{
	Handheld,
	ShipCannon
};

struct WeaponGripTransform
{
	DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 rotation = { -DirectX::XM_PIDIV2, 0.0f, 0.0f };
	DirectX::XMFLOAT3 scale = { 0.55f, 0.55f, 0.55f };
};

struct WeaponComponent
{
	WeaponType type = WeaponType::Handheld;
	float damage = 10.0f; // Damage dealt by the weapon
	float range = 100.0f; // Range of the weapon
	float fireRate = 1.0f; // Shots per second
	float currentCooldown = 0.0f; // Time since last shot fired
	float breachRadius = 0.65f; // World-space radius used by the mesh cutter.
	bool triggerHeld = false; // Presentation state; LMB does not deal damage yet.

	std::string visualModelPath = "assets/model/TestRifle.glb";
	std::string rightHandBone = "mixamorig:RightHand";
	std::string rifleIdleAnimation = "Rifle_Idle";
	std::string rifleGunAnimation = "Rifle_Gun";
	WeaponGripTransform worldGrip{};
	WeaponGripTransform firstPersonGrip{};
	float firstPersonPitchScale = 1.0f;
	float firstPersonPitchOffset = 0.0f;
	DirectX::XMFLOAT3 leftHandGrip = { 0.0f, 0.18f, 0.0f };
	DirectX::XMFLOAT3 muzzlePosition = { 0.0f, 0.43f, 0.0f };
	bool showWorldVisual = true;
	bool showFirstPersonVisual = true;
	bool enableLeftHandIK = false;

	bool CanDamageHull() const
	{
		return type == WeaponType::ShipCannon;
	}
};

struct EquippedWeaponComponent
{
	Entity weapon = INVALID_ENTITY;
};

struct WeaponOwnerComponent
{
	Entity owner = INVALID_ENTITY;
};
