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
	// Point in weapon model space that the rotation gizmo turns around.
	DirectX::XMFLOAT3 pivot = { 0.0f, 0.0f, 0.0f };
};

struct WeaponComponent
{
    // Shared by player/NPC animation selection. Kept separate from damage type.
    bool pistolPose = false;
	WeaponType type = WeaponType::Handheld;
	float damage = 10.0f; // Damage dealt by the weapon
	float range = 100.0f; // Range of the weapon
	float fireRate = 1.0f; // Shots per second
	float currentCooldown = 0.0f; // Time since last shot fired
	// Magazine/reload state is shared by the player and NPC weapons. Tactical
	// AI may read these values later, but the weapon itself owns ammunition.
	int magazineSize = 30;
	int ammoInMagazine = 30;
	int reserveAmmo = 90;
	float reloadDuration = 2.2f;
	float reloadTimer = 0.0f;
	bool isReloading = false;
	float breachRadius = 0.65f; // World-space radius used by the mesh cutter.
	bool triggerHeld = false;
	unsigned int shotsFired = 0;
	float shotFlashTime = 0.0f;
	// Полуугол конуса в градусах. В движении точность хуже, очередь расширяет конус.
	float spreadDegrees = 1.5f;
	float movingSpreadMultiplier = 2.5f;
	float recoilPerShotDegrees = 0.35f;
	float maxRecoilDegrees = 3.0f;
	float recoilRecoveryDegrees = 4.0f;
	float currentRecoilDegrees = 0.0f;
	float muzzleFlashScale = 1.0f;
	bool muzzleFlashEnabled = true;

	std::string visualModelPath = "assets/model/Rifle_M4_CC0.glb";
	// Empty means scene values only. A named profile loads from assets/config/weapons/.
	std::string gripProfileName;
	std::string rightHandBone = "mixamorig:RightHand";
	std::string rifleIdleAnimation = "Kevin_Aim_Rifle";
	std::string rifleGunAnimation = "Kevin_Aim_Shoot_R";
	WeaponGripTransform worldGrip{
		{ 0.0298783f, 0.0213676f, 0.1143637f },
		{ -0.0080474f, 0.0122972f, -1.870682f },
		{ 0.8f, 0.8f, 0.8f } };
	WeaponGripTransform firstPersonGrip{};
	float firstPersonPitchScale = 1.0f;
	float firstPersonPitchOffset = 0.0f;
	DirectX::XMFLOAT3 leftHandGrip = { 0.0f, 0.18f, 0.0f };
	DirectX::XMFLOAT3 leftHandRotation = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 firstPersonLeftHandGrip = { 0.0f, 0.18f, 0.0f };
	DirectX::XMFLOAT3 firstPersonLeftHandRotation = { 0.0f, 0.0f, 0.0f };
	std::string leftUpperArmBone = "mixamorig:LeftArm";
	std::string leftForearmBone = "mixamorig:LeftForeArm";
	std::string leftHandBone = "mixamorig:LeftHand";
	DirectX::XMFLOAT3 muzzlePosition = { 0.0f, 0.11f, 0.607f };
	bool showWorldVisual = true;
	bool showFirstPersonVisual = true;
	bool enableLeftHandIK = false;
	bool enableFirstPersonLeftHandIK = false;

	bool CanDamageHull() const
	{
		return type == WeaponType::ShipCannon;
	}
};

struct EquippedWeaponComponent
{
	Entity weapon = INVALID_ENTITY;
};

// Производная модель в руке NPC: пересоздаётся из инвентаря, в сцену не сохраняется.
struct NpcWeaponVisualComponent
{
	Entity owner = INVALID_ENTITY;
	Entity weapon = INVALID_ENTITY;
	std::string modelPath;
};

struct WeaponOwnerComponent
{
	Entity owner = INVALID_ENTITY;
};
