#include "FileSystem.h"
#include "JobSystem.h"
#include "../GameTimer.h"
#pragma warning(push)
#pragma warning(disable: 4100 4456)
#include "../src/Gameplay/Inventory/InventorySystem.h"
#include "../src/Gameplay/Power/GeneratorSystem.h"
#include "../src/Gameplay/Power/OxygenGeneratorSystem.h"
#include "../src/Gameplay/Power/ShipPowerGridSystem.h"
#include "../src/Gameplay/Player/PlayerVitalsSystem.h"
#pragma warning(pop)

// Engine rendering/physics headers include Win32, whose legacy macros collide
// with the intentionally platform-neutral FileSystem member names used below.
#ifdef CreateDirectory
#undef CreateDirectory
#endif
#ifdef DeleteFile
#undef DeleteFile
#endif

#include "../src/Animation/MeleeActionJson.h"
#include "../src/Animation/TwoBoneIK.h"
#include "../NPCComponents.h"
#include "../src/Gameplay/AI/NpcCrowdSpacing.h"
#include "../src/Gameplay/AI/NpcCombatDecisionRules.h"
#include "../src/Gameplay/AI/NpcCombatMath.h"
#include "../src/Gameplay/AI/NpcCombatModules.h"
#include "../src/Gameplay/AI/NpcCoverLifecycle.h"
#include "../src/Gameplay/AI/NpcSimulationStudio.h"
#include "../src/Gameplay/Combat/WeaponFiringRules.h"
#include "../src/Physics/PhysicsRaycast.h"

#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
    namespace fs = std::filesystem;

    class TemporaryDirectory
    {
    public:
        TemporaryDirectory()
        {
            const auto id = std::chrono::steady_clock::now()
                .time_since_epoch().count();
            m_path = fs::temp_directory_path()
                / ("dx_framework_contracts_" + std::to_string(id));
            if (!fs::create_directories(m_path))
                throw std::runtime_error("Could not create temporary test directory");
        }

        ~TemporaryDirectory()
        {
            std::error_code ignored;
            fs::remove_all(m_path, ignored);
        }

        const fs::path& Path() const { return m_path; }

    private:
        fs::path m_path;
    };

    class ScopedCurrentDirectory
    {
    public:
        explicit ScopedCurrentDirectory(const fs::path& path)
            : m_previous(fs::current_path())
        {
            fs::current_path(path);
        }

        ~ScopedCurrentDirectory()
        {
            std::error_code ignored;
            fs::current_path(m_previous, ignored);
        }

    private:
        fs::path m_previous;
    };

    int failures = 0;

    void Expect(bool condition, const char* message)
    {
        if (condition)
        {
            std::cout << "[PASS] " << message << '\n';
            return;
        }

        ++failures;
        std::cerr << "[FAIL] " << message << '\n';
    }

    void WriteTextFile(const fs::path& path, const std::string& text)
    {
        std::ofstream output(path, std::ios::binary);
        if (!output)
            throw std::runtime_error("Could not create test file");
        output << text;
    }
}

int main()
{
    try
    {
        TemporaryDirectory temporaryDirectory;
        const fs::path sandbox = temporaryDirectory.Path();
        const fs::path root = sandbox / "root";
        const fs::path outside = sandbox / "outside";
        fs::create_directories(outside);

        NPCCombatComponent meleeRange;
        {
            WeaponComponent standing, moving;
            WeaponFiringRules::CommitShot(standing, WeaponFiringRules::MotionMultiplier(standing, false));
            WeaponFiringRules::CommitShot(moving, WeaponFiringRules::MotionMultiplier(moving, true));
            Expect(standing.currentRecoilDegrees < moving.currentRecoilDegrees,
                "standing produces less recoil than moving");
            for (int i = 0; i < 100; ++i) WeaponFiringRules::CommitShot(moving, 2.5f);
            Expect(moving.currentRecoilDegrees == moving.maxRecoilDegrees, "recoil is bounded during sustained fire");
            WeaponFiringRules::AdvanceTimers(moving, 10.0f);
            Expect(moving.currentRecoilDegrees == 0.0f, "recoil recovers after firing stops");
            for (const DirectX::XMFLOAT3 axis : { DirectX::XMFLOAT3{0,0,1},
                DirectX::XMFLOAT3{0,1,0}, DirectX::XMFLOAT3{0,-1,0} })
            {
                bool bounded = true;
                float meanX = 0.0f, meanY = 0.0f, meanZ = 0.0f;
                for (int i = 0; i < 256; ++i)
                {
                    const auto ray = WeaponFiringRules::ConeDirection(axis, 5.0f,
                        (i + 0.5f) / 256.0f, i * 0.6180339887f);
                    const float length = ray.x*ray.x + ray.y*ray.y + ray.z*ray.z;
                    const float dot = ray.x*axis.x + ray.y*axis.y + ray.z*axis.z;
                    bounded &= std::abs(length-1.0f) < 0.0001f && dot >= std::cos(DirectX::XMConvertToRadians(5.0f))-0.00001f;
                    meanX += ray.x; meanY += ray.y; meanZ += ray.z;
                }
                Expect(bounded, "spread directions are unit length and stay within the configured cone including vertical aim");
                Expect(std::abs(meanX/256.0f-axis.x) < 0.01f &&
                    std::abs(meanY/256.0f-axis.y) < 0.01f && std::abs(meanZ/256.0f-axis.z) < 0.01f,
                    "spread does not bias the center of aim");
                const auto exact = WeaponFiringRules::ConeDirection(axis, 0.0f, 0.8f, 0.4f);
                Expect(std::abs(exact.x-axis.x)+std::abs(exact.y-axis.y)+std::abs(exact.z-axis.z) < 0.0001f,
                    "zero spread preserves original aim");
            }
        }
        WeaponComponent firingWeapon;
        firingWeapon.fireRate = 2.0f;
        Expect(WeaponFiringRules::CanFire(firingWeapon), "ready weapon accepts a shot");
        WeaponFiringRules::CommitShot(firingWeapon);
        Expect(firingWeapon.shotsFired == 1 && firingWeapon.currentCooldown == 0.5f &&
            !WeaponFiringRules::CanFire(firingWeapon),
            "a shot including a miss consumes cooldown and advances the animation counter");
        WeaponFiringRules::AdvanceTimers(firingWeapon, 0.25f);
        Expect(!WeaponFiringRules::CanFire(firingWeapon), "weapon cannot fire before its cooldown expires");
        WeaponFiringRules::AdvanceTimers(firingWeapon, 0.25f);
        Expect(WeaponFiringRules::CanFire(firingWeapon) && firingWeapon.shotFlashTime == 0.0f,
            "shared timer stepping makes an NPC weapon ready for its next shot");
        firingWeapon.fireRate = 0.0f;
        Expect(!WeaponFiringRules::CanFire(firingWeapon), "zero fire rate cannot divide by zero");
        Expect(!WeaponFiringRules::ValidRay({}, {}) &&
            WeaponFiringRules::ValidRay({}, { 0.0f, 0.0f, 1.0f }),
            "a zero direction is rejected before spending a shot");
        {
            btDefaultCollisionConfiguration configuration;
            btCollisionDispatcher dispatcher(&configuration);
            btDbvtBroadphase broadphase;
            btSequentialImpulseConstraintSolver solver;
            btDiscreteDynamicsWorld world(&dispatcher, &broadphase, &solver, &configuration);
            btBoxShape shape(btVector3(0.5f, 0.5f, 0.5f));
            btCollisionObject shooterBody, wall, targetBody;
            shooterBody.setCollisionShape(&shape);
            wall.setCollisionShape(&shape);
            targetBody.setCollisionShape(&shape);
            shooterBody.setUserIndex(22); wall.setUserIndex(33); targetBody.setUserIndex(44);
            btTransform pose; pose.setIdentity();
            shooterBody.setWorldTransform(pose);
            pose.setOrigin(btVector3(0,0,3)); wall.setWorldTransform(pose);
            pose.setOrigin(btVector3(0,0,6)); targetBody.setWorldTransform(pose);
            world.addCollisionObject(&shooterBody);
            world.addCollisionObject(&wall);
            world.addCollisionObject(&targetBody);
            PhysicsRaycastHit hit{};
            PhysicsRaycast::CastRay({}, {0,0,1}, &world, 10.0f, 22, &shooterBody, hit);
            Expect(hit.entity == 33, "a muzzle ray ignores the shooter and stops at cover before the target");
            world.removeCollisionObject(&wall);
            PhysicsRaycast::CastRay({}, {0,0,1}, &world, 10.0f, 22, &shooterBody, hit);
            Expect(hit.entity == 44, "the same shot ray reaches the target when cover is removed");
            const auto scattered = WeaponFiringRules::ConeDirection({0,0,1}, 10.0f, 1.0f, 0.0f);
            PhysicsRaycast::CastRay({}, scattered, &world, 10.0f, 22, &shooterBody, hit);
            Expect(hit.collisionObject == nullptr,
                "spread changes the physical ray and can miss a target that centered aim would hit");
            world.removeCollisionObject(&targetBody);
            world.removeCollisionObject(&shooterBody);
        }
        NPCCombatComponent searchMemory;
        {
            NPCCombatComponent looking;
            looking.BeginSearchTurn();
            const float firstYaw = looking.AdvanceSearchTurn(0.0f, 2.0f, 0.1f);
            Expect(std::abs(firstYaw - 0.2f) < 0.0001f,
                "a stationary searching NPC turns without needing a displacement to the target");
            float yaw = firstYaw;
            float totalTurn = 0.2f;
            for (int i = 0; i < 100; ++i)
            {
                const float before = looking.searchTurnRemaining;
                yaw = looking.AdvanceSearchTurn(yaw, 2.0f, 0.1f);
                totalTurn += before - looking.searchTurnRemaining;
            }
            Expect(looking.searchTurnRemaining == 0.0f &&
                std::abs(totalTurn - DirectX::XM_2PI) < 0.001f && std::abs(yaw) < 0.001f,
                "search scans the rear and completes exactly one bounded full turn");
            Expect(looking.AdvanceSearchTurn(yaw, 2.0f, 0.1f) == yaw,
                "completed search does not spin indefinitely");
            looking.BeginSearchTurn();
            Expect(looking.AdvanceSearchTurn(0.0f, 2.0f, 0.0f) == 0.0f &&
                looking.searchTurnRemaining == DirectX::XM_2PI,
                "paused simulation does not consume the search turn");
            looking.ObserveVisibleTarget({0,0,1}, 0.1f, false);
            Expect(looking.searchTurnRemaining == 0.0f,
                "reacquiring the target immediately cancels the search turn");
        }
        searchMemory.ObserveVisibleTarget({ 0.0f, 2.0f, 0.0f }, 0.1f, false);
        searchMemory.ObserveVisibleTarget({ 0.5f, 2.0f, 0.0f }, 0.1f, true);
        const auto predictedEscape = searchMemory.EstimateSearchPosition();
        Expect(std::abs(predictedEscape.x - 3.5f) < 0.001f && predictedEscape.y == 2.0f,
            "search extrapolates visible velocity with a bounded horizontal distance");
        Expect(searchMemory.lastKnownPosition.x == 0.5f &&
            searchMemory.EstimateSearchPosition().x == predictedEscape.x,
            "prediction stays fixed and does not overwrite the last observed position");
        searchMemory.ObserveVisibleTarget({ 20.0f, 2.0f, 0.0f }, 0.1f, false);
        Expect(searchMemory.EstimateSearchPosition().x == 20.0f,
            "reacquiring a target does not infer its unseen travel velocity");
        searchMemory.ObserveVisibleTarget({ 20.0f, 2.0f, 0.0f }, 0.1f, true);
        Expect(searchMemory.EstimateSearchPosition().x == 20.0f,
            "a stationary observed target does not produce a false escape path");
        Expect(meleeRange.ShouldHoldMeleeStance(1.49f * 1.49f, false),
            "melee pursuit enters stance at striking distance");
        meleeRange.EnterMeleeStance();
        Expect(meleeRange.ShouldHoldMeleeStance(4.0f, true) &&
            meleeRange.attackTimer >= 0.30f,
            "entering melee guard holds the pose and delays the first punch");
        meleeRange.stanceTimeRemaining = 0.0f;
        Expect(meleeRange.ShouldHoldMeleeStance(1.65f * 1.65f, true) &&
            !meleeRange.ShouldHoldMeleeStance(1.65f * 1.65f, false),
            "small target movements do not flip stance back to pursuit");
        Expect(!meleeRange.ShouldHoldMeleeStance(1.90f * 1.90f, true),
            "pursuit resumes once the target leaves the stance exit range");

        GameTimer gameTimer;
        Expect(gameTimer.Update(0.5f) == 0.5f &&
            gameTimer.GetTotalSeconds() == 0.5,
            "game clock advances by the scaled simulation delta");
        gameTimer.SetTimeScale(2.0f);
        Expect(gameTimer.Update(0.25f) == 0.5f &&
            gameTimer.GetTotalSeconds() == 1.0,
            "game clock time scale changes both delta and total time");
        gameTimer.SetPaused(true);
        Expect(gameTimer.Update(1.0f) == 0.0f &&
            gameTimer.GetTotalSeconds() == 1.0,
            "paused game clock freezes simulation time");
        gameTimer.Reset(3.0);
        Expect(!gameTimer.IsPaused() && gameTimer.GetTimeScale() == 1.0f &&
            gameTimer.GetTotalSeconds() == 3.0,
            "game clock reset starts a clean deterministic play session");
        gameTimer.SetTimeScale(-5.0f);
        Expect(gameTimer.GetTimeScale() == 0.0f,
            "game clock rejects a negative time scale");
        gameTimer.SetTimeScale((std::numeric_limits<float>::quiet_NaN)());
        Expect(gameTimer.GetTimeScale() == 1.0f &&
            gameTimer.Update((std::numeric_limits<float>::infinity)()) == 0.0f,
            "game clock contains non-finite timing input");
        gameTimer.Reset((std::numeric_limits<double>::quiet_NaN)());
        Expect(gameTimer.GetTotalSeconds() == 0.0,
            "game clock rejects a non-finite reset time");

        const MeleeActionJson::json poseProfile = {
            { "pose_keys", MeleeActionJson::json::array({
                {
                    { "clip", "Kevin_Walk" },
                    { "bone", "mixamorig:Spine" },
                    { "variant", -1 },
                    { "time", 0.25f },
                    { "position", { 1.0f, 2.0f, 3.0f } },
                    { "rotation", { 10.0f, 20.0f, 30.0f } }
                },
                {
                    { "clip", "Kevin_Walk" },
                    { "bone", "mixamorig:Head" },
                    { "time", (std::numeric_limits<float>::quiet_NaN)() }
                },
                {
                    { "clip", "Kevin_Walk" },
                    { "bone", "mixamorig:LeftHand" },
                    { "time", 0.5f },
                    { "position", {
                        (std::numeric_limits<float>::infinity)(), 0.0f, 0.0f } }
                },
                {
                    { "clip", "Kevin_Walk" },
                    { "bone", "mixamorig:RightHand" },
                    { "time", 0.75f },
                    { "rotation", { 0.0f,
                        (std::numeric_limits<float>::quiet_NaN)(), 0.0f } }
                },
                {
                    { "clip", "Kevin_Walk" },
                    { "bone", "mixamorig:LeftArm" },
                    { "variant", "bad" },
                    { "time", 0.2f }
                },
                {
                    { "clip", "Kevin_Walk" },
                    { "bone", "mixamorig:RightArm" },
                    { "time", "bad" }
                }
            }) },
            { "first_person_pose_keys", MeleeActionJson::json::array({
                {
                    { "clip", "Kevin_Melee_Jab_L" },
                    { "bone", "mixamorig:LeftHand" },
                    { "variant", 2 },
                    { "time", 0.4f },
                    { "position", { -0.1f, 0.2f, 0.3f } },
                    { "rotation", { -5.0f, 15.0f, 25.0f } }
                }
            }) },
            { "fist_keys", MeleeActionJson::json::array({
                {
                    { "clip", "Kevin_Walk" }, { "variant", -1 },
                    { "time", 0.3f }, { "right_hand", true },
                    { "amount", 1.5f }
                },
                {
                    { "clip", "Kevin_Walk" }, { "time", 0.4f },
                    { "right_hand", "bad" }, { "amount", 0.5f }
                },
                {
                    { "clip", "Kevin_Walk" }, { "time", 0.5f },
                    { "amount", (std::numeric_limits<float>::quiet_NaN)() }
                }
            }) },
            { "first_person_fist_keys", MeleeActionJson::json::array({
                {
                    { "clip", "Kevin_Melee_Jab_L" }, { "variant", 2 },
                    { "time", 0.4f }, { "right_hand", false },
                    { "amount", -0.75f }
                }
            }) }
        };

        const fs::path animationConfigDirectory =
            sandbox / "assets" / "config" / "animations";
        fs::create_directories(animationConfigDirectory);
        const MeleeActionJson::json diskPoseProfile = {
            { "pose_keys", MeleeActionJson::json::array({
                poseProfile["pose_keys"].front() }) },
            { "first_person_pose_keys", MeleeActionJson::json::array({
                poseProfile["first_person_pose_keys"].front() }) },
            { "fist_keys", poseProfile["fist_keys"] },
            { "first_person_fist_keys", poseProfile["first_person_fist_keys"] }
        };
        WriteTextFile(animationConfigDirectory / "pose_contract.json",
            diskPoseProfile.dump(2));

        AnimationComponent filteredPoseAnimation;
        Expect(MeleeActionJson::ReadPoseKeys(
                poseProfile, filteredPoseAnimation, "pose_keys") &&
            filteredPoseAnimation.poseKeys.size() == 1 &&
            filteredPoseAnimation.poseKeys.front().bone == "mixamorig:Spine",
            "pose key parsing rejects malformed and non-finite transforms");
        Expect(MeleeActionJson::ReadFistKeys(
                poseProfile, filteredPoseAnimation, "fist_keys") &&
            filteredPoseAnimation.fistKeys.size() == 1 &&
            filteredPoseAnimation.fistKeys.front().rightHand &&
            filteredPoseAnimation.fistKeys.front().amount == 1.0f,
            "fist key parsing rejects malformed values and clamps the controller");

        const MeleeActionJson::json punchIKProfile = {
            { "upper_body_root", "mixamorig:Spine1" },
            { "actions", MeleeActionJson::json::array({
                {
                    { "id", "unarmed_punch" },
                    { "type", "weighted_punch" },
                    { "requires_guard", true },
                    { "base_world_clip", "Kevin_Melee_Guard" }
                }
            }) },
            { "punch_variants", MeleeActionJson::json::array({
                {
                    { "name", "NPC Jab" },
                    { "world_clip", "Kevin_Melee_Jab_L" },
                    { "right_hand", false },
                    { "ik_strength", 1.25f },
                    { "fist_offset", { 0.01f, -0.02f, 0.03f } },
                    { "hand_rotation", { 15.0f, -25.0f, 35.0f } },
                    { "aim_offset", { -0.04f, 0.05f, 0.06f } },
                    { "elbow_pole_offset", { 0.2f, -0.1f, 0.3f } },
                    { "arm_extension", 1.5f },
                    { "ik_start_phase", 0.2f },
                    { "ik_end_phase", 0.8f }
                }
            }) }
        };
        AnimationComponent punchIKAnimation;
        MeleeActionJson::Read(punchIKProfile, punchIKAnimation,
            "punch_variants");
        Expect(punchIKAnimation.punchVariants.size() == 1 &&
            punchIKAnimation.upperBodyRoot == "mixamorig:Spine1" &&
            punchIKAnimation.punchVariants.front().ikStrength == 1.0f &&
            punchIKAnimation.punchVariants.front().fistOffset.y == -0.02f &&
            punchIKAnimation.punchVariants.front().handRotation.z == 35.0f &&
            punchIKAnimation.punchVariants.front().aimOffset.x == -0.04f &&
            punchIKAnimation.punchVariants.front().elbowPoleOffset.z == 0.3f &&
            punchIKAnimation.punchVariants.front().armExtension == 0.99f &&
            punchIKAnimation.punchVariants.front().ikStartPhase == 0.2f &&
            punchIKAnimation.punchVariants.front().ikEndPhase == 0.8f,
            "punch aim, wrist, and elbow IK tuning load per variant");
        Expect(punchIKAnimation.meleeActions.attackBaseWorldClip ==
                "Kevin_Melee_Guard",
            "combat profile loads the full-body base clip under attacks");
        MeleeActionJson::json savedPunchIKProfile;
        MeleeActionJson::Write(savedPunchIKProfile, punchIKAnimation,
            "punch_variants");
        Expect(savedPunchIKProfile["punch_variants"][0]["ik_strength"] == 1.0f &&
            savedPunchIKProfile["upper_body_root"] == "mixamorig:Spine1" &&
            savedPunchIKProfile["punch_variants"][0]["fist_offset"][0] == 0.01f &&
            savedPunchIKProfile["punch_variants"][0]["hand_rotation"][1] == -25.0f &&
            savedPunchIKProfile["punch_variants"][0]["aim_offset"][2] == 0.06f &&
            savedPunchIKProfile["punch_variants"][0]["elbow_pole_offset"][0] == 0.2f &&
            savedPunchIKProfile["punch_variants"][0]["arm_extension"] == 0.99f &&
            savedPunchIKProfile["punch_variants"][0]["ik_start_phase"] == 0.2f &&
            savedPunchIKProfile["punch_variants"][0]["ik_end_phase"] == 0.8f,
            "punch IK tuning survives profile serialization");
        Expect(savedPunchIKProfile["actions"][1]["base_world_clip"] ==
                "Kevin_Melee_Guard",
            "combat profile saves the full-body base clip under attacks");

        {
            using namespace DirectX;
            const XMVECTOR shoulder = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
            const XMVECTOR elbow = XMVectorSet(0.0f, -0.5f, 0.25f, 1.0f);
            const XMVECTOR wrist = XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f);
            const XMVECTOR target = XMVectorSet(0.0f, 0.0f, 0.9f, 1.0f);
            const XMVECTOR poleDown = XMVectorSet(0.0f, -1.0f, 0.0f, 1.0f);
            const XMVECTOR poleUp = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
            XMVECTOR elbowDown{};
            XMVECTOR wristDown{};
            XMVECTOR elbowUp{};
            XMVECTOR wristUp{};
            const bool solvedDown = TwoBoneIK::Solve(shoulder, elbow, wrist,
                target, elbowDown, wristDown, &poleDown);
            const bool solvedUp = TwoBoneIK::Solve(shoulder, elbow, wrist,
                target, elbowUp, wristUp, &poleUp);
            Expect(solvedDown && solvedUp &&
                XMVectorGetY(elbowDown) < -0.01f &&
                XMVectorGetY(elbowUp) > 0.01f,
                "two-bone IK pole direction rotates the elbow bend plane");
        }

        AnimationComponent worldPoseAnimation;
        AnimationComponent firstPersonPoseAnimation;
        bool loadedWorldProfile = false;
        bool loadedFirstPersonProfile = false;
        {
            ScopedCurrentDirectory profileRoot(sandbox);
            loadedWorldProfile = MeleeActionJson::LoadProfile(
                "pose_contract", worldPoseAnimation, false);
            loadedFirstPersonProfile = MeleeActionJson::LoadProfile(
                "pose_contract", firstPersonPoseAnimation, true);
        }
        Expect(loadedWorldProfile && worldPoseAnimation.poseKeys.size() == 1 &&
            worldPoseAnimation.poseKeys.front().clip == "Kevin_Walk" &&
            worldPoseAnimation.poseKeys.front().bone == "mixamorig:Spine" &&
            worldPoseAnimation.poseKeys.front().position.y == 2.0f,
            "world profile loading selects third-person pose keys");
        Expect(worldPoseAnimation.fistKeys.size() == 1 &&
            worldPoseAnimation.fistKeys.front().clip == "Kevin_Walk" &&
            worldPoseAnimation.fistKeys.front().rightHand,
            "world profile loading selects third-person fist keys");
        Expect(loadedFirstPersonProfile &&
            firstPersonPoseAnimation.poseKeys.size() == 1 &&
            firstPersonPoseAnimation.poseKeys.front().clip ==
                "Kevin_Melee_Jab_L" &&
            firstPersonPoseAnimation.poseKeys.front().variant == 2 &&
            worldPoseAnimation.poseKeys.front().clip !=
                firstPersonPoseAnimation.poseKeys.front().clip,
            "first-person pose keys load from their own profile section");
        Expect(firstPersonPoseAnimation.fistKeys.size() == 1 &&
            firstPersonPoseAnimation.fistKeys.front().clip ==
                "Kevin_Melee_Jab_L" &&
            !firstPersonPoseAnimation.fistKeys.front().rightHand &&
            firstPersonPoseAnimation.fistKeys.front().amount == -0.75f,
            "first-person fist keys load from their own profile section");

        AnimationComponent unchangedPoseAnimation;
        unchangedPoseAnimation.poseKeys.push_back(
            { "Existing", "ExistingBone", -1, 0.1f, {}, {} });
        Expect(!MeleeActionJson::ReadPoseKeys(
                poseProfile, unchangedPoseAnimation, "missing_pose_keys") &&
            unchangedPoseAnimation.poseKeys.size() == 1 &&
            unchangedPoseAnimation.poseKeys.front().clip == "Existing",
            "a missing pose section does not erase scene-authored keys");

        ECSRegistry inventoryRegistry;
        const Entity inventoryOwner = inventoryRegistry.CreateEntity();
        std::array<Entity, 4> testWeapons{};
        for (Entity& weapon : testWeapons)
        {
            weapon = inventoryRegistry.CreateEntity();
            inventoryRegistry.AddComponent<WeaponComponent>(
                weapon, WeaponComponent{});
        }

        Expect(InventorySystem::EnsureInventory(
            inventoryRegistry, inventoryOwner) != nullptr,
            "inventory creates a fixed three-slot container for a valid owner");
        for (std::size_t slot = 0; slot < InventoryComponent::SlotCount; ++slot)
        {
            const InventoryOperation pickup = InventorySystem::PickupWeapon(
                inventoryRegistry, inventoryOwner, testWeapons[slot]);
            Expect(pickup && pickup.slot == slot,
                "inventory stores a weapon in the next available slot");
        }
        Expect(
            InventorySystem::PickupWeapon(
                inventoryRegistry,
                inventoryOwner,
                testWeapons.back()).status == InventoryStatus::InventoryFull,
            "inventory refuses a fourth weapon without losing the world item");

        const InventoryOperation selected = InventorySystem::SelectSlot(
            inventoryRegistry, inventoryOwner, 1);
        const auto* equippedAfterSelection =
            inventoryRegistry.GetComponent<EquippedWeaponComponent>(inventoryOwner);
        Expect(selected && equippedAfterSelection &&
            equippedAfterSelection->weapon == testWeapons[1],
            "inventory selection synchronizes the equipped weapon link");

        const InventoryOperation dropped = InventorySystem::DropActive(
            inventoryRegistry, inventoryOwner);
        const auto* inventoryAfterDrop =
            inventoryRegistry.GetComponent<InventoryComponent>(inventoryOwner);
        const auto* equippedAfterDrop =
            inventoryRegistry.GetComponent<EquippedWeaponComponent>(inventoryOwner);
        Expect(dropped && inventoryAfterDrop &&
            inventoryAfterDrop->GetItem(1) == INVALID_ENTITY &&
            equippedAfterDrop && equippedAfterDrop->weapon == testWeapons[0] &&
            !inventoryRegistry.HasComponent<WeaponOwnerComponent>(testWeapons[1]),
            "dropping a weapon clears ownership and selects the next stored rifle");

        ECSRegistry powerRegistry;
        const Entity generatorEntity = powerRegistry.CreateEntity();
        GeneratorComponent generator{};
        generator.isOnline = true;
        generator.currentOutputMw = 20.0f;
        generator.targetOutputMw = 20.0f;
        generator.deuteriumKg = 10.0f;
        generator.tritiumKg = 10.0f;
        powerRegistry.AddComponent<GeneratorComponent>(
            generatorEntity, generator);
        powerRegistry.AddComponent<ActiveComponent>(
            generatorEntity, ActiveComponent{});

        const Entity consumerEntity = powerRegistry.CreateEntity();
        PowerConsumerComponent consumer{};
        consumer.requiredMw = 5.0f;
        powerRegistry.AddComponent<PowerConsumerComponent>(
            consumerEntity, consumer);
        powerRegistry.AddComponent<ActiveComponent>(
            consumerEntity, ActiveComponent{});

        ShipPowerGridSystem::Update(powerRegistry, 0.1f);
        Expect(powerRegistry.GetComponent<PowerConsumerComponent>(consumerEntity)
            ->hasPower,
            "an active generator supplies an active power consumer");

        const float fuelBeforeDisable =
            powerRegistry.GetComponent<GeneratorComponent>(generatorEntity)
            ->deuteriumKg;
        powerRegistry.RemoveComponent<ActiveComponent>(generatorEntity);
        GeneratorSystem::Update(powerRegistry, 1.0f);
        ShipPowerGridSystem::Update(powerRegistry, 1.0f);
        const auto* disabledGenerator =
            powerRegistry.GetComponent<GeneratorComponent>(generatorEntity);
        const auto* unpoweredConsumer =
            powerRegistry.GetComponent<PowerConsumerComponent>(consumerEntity);
        Expect(disabledGenerator && unpoweredConsumer &&
            disabledGenerator->currentOutputMw == 0.0f &&
            disabledGenerator->deuteriumKg == fuelBeforeDisable &&
            !unpoweredConsumer->hasPower,
            "an inactive generator neither burns fuel nor supplies power");

        powerRegistry.RemoveComponent<ActiveComponent>(consumerEntity);
        ShipPowerGridSystem::Update(powerRegistry, 0.1f);
        Expect(!powerRegistry.GetComponent<PowerConsumerComponent>(consumerEntity)
            ->hasPower,
            "an inactive consumer is excluded and its runtime power state is reset");

        ECSRegistry oxygenRegistry;
        const Entity oxygenGeneratorEntity = oxygenRegistry.CreateEntity();
        OxygenGeneratorComponent oxygenGenerator{};
        oxygenGenerator.enabled = true;
        oxygenGenerator.hasPower = true;
        oxygenGenerator.oxygenPerSecond = 0.25f;
        oxygenRegistry.AddComponent<OxygenGeneratorComponent>(
            oxygenGeneratorEntity, oxygenGenerator);
        oxygenRegistry.AddComponent<ActiveComponent>(
            oxygenGeneratorEntity, ActiveComponent{});

        const Entity roomEntity = oxygenRegistry.CreateEntity();
        RoomComponent room{};
        room.isActive = true;
        room.oxygen = 0.0f;
        room.pressure = 1.0f;
        oxygenRegistry.AddComponent<RoomComponent>(roomEntity, room);
        oxygenRegistry.AddComponent<ActiveComponent>(roomEntity, ActiveComponent{});
        OxygenGeneratorSystem::Update(oxygenRegistry, 1.0f);
        const float oxygenAfterActiveUpdate =
            oxygenRegistry.GetComponent<RoomComponent>(roomEntity)->oxygen;
        Expect(oxygenAfterActiveUpdate > 0.0f,
            "an active oxygen generator restores an active room");

        oxygenRegistry.RemoveComponent<ActiveComponent>(oxygenGeneratorEntity);
        OxygenGeneratorSystem::Update(oxygenRegistry, 1.0f);
        Expect(oxygenRegistry.GetComponent<RoomComponent>(roomEntity)->oxygen ==
            oxygenAfterActiveUpdate,
            "an inactive oxygen generator produces no atmosphere");

        ECSRegistry staminaRegistry;
        const Entity staminaEntity = staminaRegistry.CreateEntity();
        CharacterVitalsComponent staminaVitals{};
        staminaVitals.stamina = 1.0f;
        staminaVitals.staminaRecoveryPerSecond = 10.0f;
        staminaVitals.staminaDrainPerSecond = 20.0f;
        CharacterMovement staminaMovement{};
        staminaMovement.wantsRun = true;
        staminaMovement.isRunning = false;
        staminaRegistry.AddComponent<CharacterVitalsComponent>(
            staminaEntity, staminaVitals);
        staminaRegistry.AddComponent<CharacterMovement>(
            staminaEntity, staminaMovement);
        staminaRegistry.AddComponent<ActiveComponent>(
            staminaEntity, ActiveComponent{});

        PlayerVitalsSystem::Update(staminaRegistry, 0.5f);
        Expect(staminaRegistry.GetComponent<CharacterVitalsComponent>(
            staminaEntity)->stamina == 6.0f,
            "stamina recovers when sprint is requested but cannot run");

        staminaRegistry.GetComponent<CharacterMovement>(staminaEntity)
            ->isRunning = true;
        PlayerVitalsSystem::Update(staminaRegistry, 0.25f);
        Expect(staminaRegistry.GetComponent<CharacterVitalsComponent>(
            staminaEntity)->stamina == 1.0f,
            "stamina drains while the character is actually running");

        FileSystem invalidFileSystem(fs::path{});
        Expect(invalidFileSystem.GetRootPath().empty(),
            "an empty root is rejected instead of resolving to the working directory");
        Expect(!invalidFileSystem.CreateDirectory("unexpected"),
            "operations fail when the root is invalid");

        FileSystem fileSystem(root);
        Expect(fs::is_directory(fileSystem.GetRootPath()),
            "constructor creates and canonicalizes the root directory");

        Expect(fileSystem.CreateDirectory("textures/ui"),
            "directory creation works inside the root");
        Expect(fs::is_directory(root / "textures" / "ui"),
            "created directory stays inside the root");

        WriteTextFile(root / "textures" / "ui" / "Button.TXT", "button");
        WriteTextFile(outside / "keep.txt", "outside");
        fileSystem.ScanFileSystem();

        Expect(fileSystem.GetFilesByExtension("txt").size() == 1,
            "extension lookup is case-insensitive");
        Expect(fileSystem.GetEntriesInDirectory("textures/ui").size() == 1,
            "directory listing returns direct children only");

        Expect(!fileSystem.CreateDirectory("../escaped"),
            "parent traversal is rejected for directory creation");
        Expect(!fileSystem.CreateDirectory(outside / "absolute"),
            "absolute paths are rejected for directory creation");
        Expect(!fileSystem.DeleteFile("../outside/keep.txt"),
            "parent traversal is rejected for file deletion");
        Expect(!fileSystem.DeleteFile(outside / "keep.txt"),
            "absolute paths are rejected for file deletion");
        Expect(!fileSystem.DeleteDirectory(outside),
            "absolute paths are rejected for recursive deletion");
        Expect(!fileSystem.DeleteDirectory("."),
            "the project root itself cannot be deleted");
        Expect(fs::exists(outside / "keep.txt"),
            "rejected operations leave outside data untouched");

        std::error_code symlinkError;
        fs::create_directory_symlink(outside, root / "outside_link", symlinkError);
        if (!symlinkError)
        {
            Expect(!fileSystem.DeleteFile("outside_link/keep.txt"),
                "a symlink cannot escape the project root");
            Expect(!fileSystem.DeleteDirectory("outside_link"),
                "an outside directory symlink cannot be recursively deleted");
            Expect(!fileSystem.CreateDirectory("outside_link/escaped"),
                "directory creation cannot escape through a symlink");
            Expect(fs::exists(outside / "keep.txt"),
                "symlink escape rejection preserves the target");
        }
        else
        {
            std::cout << "[SKIP] symlink escape test (OS privilege unavailable)\n";
        }

        Expect(fileSystem.DeleteFile("textures/ui/Button.TXT"),
            "file deletion works inside the root");
        Expect(fileSystem.DeleteDirectory("textures"),
            "recursive directory deletion works inside the root");
        Expect(!fs::exists(root / "textures"),
            "deleted in-root directory is gone");

        JobSystem jobSystem;
        std::atomic<int> completedJobs = 0;
        Expect(!jobSystem.SubmitJob([&completedJobs] { ++completedJobs; }),
            "a job system rejects work before initialization");
        Expect(!jobSystem.Initialize(0),
            "a job system rejects a non-positive worker count");
        Expect(jobSystem.Initialize(2),
            "a job system starts worker threads");

        bool acceptedAllJobs = true;
        for (int jobIndex = 0; jobIndex < 8; ++jobIndex)
        {
            acceptedAllJobs = jobSystem.SubmitJob(
                [&completedJobs] { ++completedJobs; }) && acceptedAllJobs;
        }
        Expect(acceptedAllJobs,
            "a running job system accepts work");
        jobSystem.Wait();
        Expect(completedJobs.load() == 8,
            "worker threads complete all queued work");

        jobSystem.Shutdown();
        Expect(!jobSystem.SubmitJob([&completedJobs] { ++completedJobs; }),
            "a shut-down job system rejects new work");
        Expect(jobSystem.Initialize(2),
            "a job system can be initialized again after shutdown");
        Expect(jobSystem.SubmitJob([&completedJobs] { ++completedJobs; }),
            "a reinitialized job system accepts work");
        jobSystem.Wait();
        Expect(completedJobs.load() == 9,
            "a reinitialized job system executes work");
        jobSystem.Shutdown();
    }
    catch (const std::exception& exception)
    {
        std::cerr << "[FATAL] " << exception.what() << '\n';
        return 2;
    }

    // Регрессия: NPC должны расходиться даже при полностью совпавших центрах.
    const auto overlapA = NpcPairSeparation(1, 2, 0.0f, 0.0f, 1.4f);
    const auto overlapB = NpcPairSeparation(2, 1, 0.0f, 0.0f, 1.4f);
    Expect(overlapA.x < 0.0f && overlapB.x > 0.0f && overlapA.x == -overlapB.x,
        "overlapping NPCs choose opposite finite separation directions");
    const auto distant = NpcPairSeparation(1, 2, 3.0f, 0.0f, 1.4f);
    Expect(distant.x == 0.0f && distant.z == 0.0f, "distant NPCs do not alter each other's route");
    const auto close = NpcPairSeparation(1, 2, 0.5f, -0.2f, 1.4f);
    Expect(close.x > 0.0f && close.z < 0.0f, "crowd correction points away from the neighbor");
    Expect(!NpcCombatDecisionRules::NeedsNewAngle(true, false),
        "clear firing position is held instead of periodic unnecessary flanking");
    Expect(NpcCombatDecisionRules::NeedsNewAngle(false, false),
        "lost personal sight permits a new firing angle");
    Expect(NpcCombatDecisionRules::NeedsNewAngle(true, true),
        "incoming damage permits repositioning despite visible target");
    Expect(NpcCombatDecisionRules::CanUseMovingFire(NpcState::Chasing, true),
        "visible target can be fired on while chasing");
    Expect(!NpcCombatDecisionRules::CanUseMovingFire(NpcState::Chasing, false),
        "radio contact alone does not permit moving fire");
    Expect(NpcCombatDecisionRules::CanUseMovingFire(NpcState::TakingCover, true),
        "visible target can be fired on while moving between cover positions");
    Expect(!NpcCombatDecisionRules::CanUseMovingFire(NpcState::Searching, true),
        "ordinary search movement does not permit speculative fire");

    NpcCombatModulesComponent minimalModules;
    Expect(minimalModules.perception && minimalModules.engagement &&
        minimalModules.pursuit && minimalModules.stationaryFire,
        "new NPC combat modules start with the complete minimal duel");
    Expect(!minimalModules.search && !minimalModules.coverSelection &&
        !minimalModules.peekCycle && !minimalModules.movingFire,
        "advanced combat modules start disabled for isolated testing");

    minimalModules.ApplyPreset(NpcCombatModulePreset::CoverAndPeek);
    Expect(minimalModules.coverSelection && minimalModules.peekCycle &&
        minimalModules.incomingFireReaction,
        "cover preset enables the complete protected peek dependency chain");
    Expect(!minimalModules.coverReposition && !minimalModules.dynamicCover &&
        !minimalModules.movingFire,
        "cover preset keeps unrelated advanced modules disabled");

    minimalModules.ApplyPreset(NpcCombatModulePreset::FullIndividual);
    Expect(minimalModules.search && minimalModules.automaticTactics &&
        minimalModules.coverReposition && minimalModules.dynamicCover &&
        minimalModules.movingFire,
        "full individual preset enables every individual combat layer");
    Expect(!minimalModules.squadTactics,
        "full individual preset does not silently enable the parked squad layer");

    minimalModules.ApplyPreset(NpcCombatModulePreset::ManualSandbox);
    Expect(minimalModules.EnabledMask() == 0,
        "manual sandbox disables all autonomous combat modules");

    minimalModules.ApplyPreset(NpcCombatModulePreset::Legacy);
    Expect(minimalModules.squadTactics && minimalModules.search &&
        minimalModules.coverSelection && minimalModules.movingFire,
        "legacy preset preserves all previously available combat layers");

    NpcCombatModulesComponent debugOverrideModules;
    debugOverrideModules.ApplyPreset(NpcCombatModulePreset::ManualSandbox);
    Expect(!debugOverrideModules.IsEnabled(NpcCombatModule::PeekCycle) &&
        !debugOverrideModules.IsOperational(NpcCombatModule::PeekCycle),
        "a disabled combat module is not operational without a debug override");

    const std::uint32_t forcedCoverMask =
        NpcCombatModuleBit(NpcCombatModule::CoverSelection) |
        NpcCombatModuleBit(NpcCombatModule::PeekCycle);
    debugOverrideModules.ForceForDebug(forcedCoverMask, 1.5f);
    Expect(!debugOverrideModules.IsEnabled(NpcCombatModule::CoverSelection) &&
        !debugOverrideModules.IsEnabled(NpcCombatModule::PeekCycle),
        "a debug override does not mutate the saved automatic module settings");
    Expect(debugOverrideModules.IsOperational(
            NpcCombatModule::CoverSelection) &&
        debugOverrideModules.IsOperational(NpcCombatModule::PeekCycle),
        "a debug override makes every requested module temporarily operational");
    Expect(!debugOverrideModules.IsOperational(NpcCombatModule::Search) &&
        !debugOverrideModules.IsOperational(NpcCombatModule::MovingFire),
        "a debug override does not enable unrelated combat modules");
    Expect(debugOverrideModules.runtimeForcedMask == forcedCoverMask &&
        std::fabs(debugOverrideModules.runtimeForcedTime - 1.5f) < 0.0001f,
        "a debug override records the requested mask and finite duration");

    debugOverrideModules.ClearDebugOverride();
    Expect(debugOverrideModules.runtimeForcedMask == 0 &&
        debugOverrideModules.runtimeForcedTime == 0.0f &&
        !debugOverrideModules.IsOperational(NpcCombatModule::PeekCycle),
        "clearing a debug override immediately restores the disabled state");

    NpcCombatModulesComponent invalidDebugDurationModules;
    invalidDebugDurationModules.ApplyPreset(
        NpcCombatModulePreset::ManualSandbox);
    invalidDebugDurationModules.ForceForDebug(
        NpcCombatModuleBit(NpcCombatModule::Search), -1.0f);
    Expect(invalidDebugDurationModules.runtimeForcedMask == 0 &&
        invalidDebugDurationModules.runtimeForcedTime == 0.0f &&
        !invalidDebugDurationModules.IsOperational(NpcCombatModule::Search),
        "a negative debug duration leaves no dormant forced-module bits");
    invalidDebugDurationModules.ForceForDebug(
        NpcCombatModuleBit(NpcCombatModule::MovingFire),
        (std::numeric_limits<float>::infinity)());
    Expect(invalidDebugDurationModules.runtimeForcedMask == 0 &&
        invalidDebugDurationModules.runtimeForcedTime == 0.0f &&
        !invalidDebugDurationModules.IsOperational(NpcCombatModule::MovingFire),
        "a non-finite debug duration is rejected without a latent override");

    NpcCombatModulesComponent presetDuringDebugOverride;
    presetDuringDebugOverride.ApplyPreset(
        NpcCombatModulePreset::ManualSandbox);
    presetDuringDebugOverride.ForceForDebug(
        NpcCombatModuleBit(NpcCombatModule::PeekCycle), 2.0f);
    presetDuringDebugOverride.ApplyPreset(
        NpcCombatModulePreset::MinimalDuel);
    Expect(!presetDuringDebugOverride.IsEnabled(NpcCombatModule::PeekCycle) &&
        !presetDuringDebugOverride.IsOperational(NpcCombatModule::PeekCycle) &&
        presetDuringDebugOverride.runtimeForcedMask == 0 &&
        presetDuringDebugOverride.IsOperational(NpcCombatModule::Perception),
        "applying a preset clears temporary overrides and restores a clean baseline");

    NpcCombatModulesComponent expiringDebugOverride;
    expiringDebugOverride.ApplyPreset(NpcCombatModulePreset::ManualSandbox);
    expiringDebugOverride.ForceForDebug(
        NpcCombatModuleBit(NpcCombatModule::Pursuit), 1.0f);
    expiringDebugOverride.TickDebugOverride(0.4f);
    Expect(expiringDebugOverride.IsOperational(NpcCombatModule::Pursuit) &&
        std::fabs(expiringDebugOverride.runtimeForcedTime - 0.6f) < 0.0001f,
        "a forced module remains operational until its debug duration expires");
    expiringDebugOverride.TickDebugOverride(0.6f);
    Expect(!expiringDebugOverride.IsOperational(NpcCombatModule::Pursuit) &&
        expiringDebugOverride.runtimeForcedMask == 0,
        "an expired debug override clears its mask instead of leaving latent state");

    Expect(NpcCombatModuleOperational(
            static_cast<const NpcCombatModulesComponent*>(nullptr),
            NpcCombatModule::SquadTactics),
        "an NPC without the modular component retains legacy operational behavior");

    NPCCombatComponent forgottenCombatMemory;
    forgottenCombatMemory.targetEntity = 77;
    forgottenCombatMemory.targetVisible = true;
    forgottenCombatMemory.hasLastKnownPosition = true;
    forgottenCombatMemory.searchPlanReady = true;
    forgottenCombatMemory.investigatingPrediction = true;
    forgottenCombatMemory.rangedAimTime = 3.0f;
    forgottenCombatMemory.ForgetTarget();
    Expect(forgottenCombatMemory.targetEntity == INVALID_ENTITY &&
        !forgottenCombatMemory.targetVisible &&
        !forgottenCombatMemory.hasLastKnownPosition &&
        !forgottenCombatMemory.searchPlanReady &&
        !forgottenCombatMemory.investigatingPrediction &&
        forgottenCombatMemory.rangedAimTime == 0.0f,
        "forgetting a combat target clears vision, search and aiming memory together");

    NpcTacticalComponent coverLifecycle;
    coverLifecycle.failedPeekCount = 4;
    coverLifecycle.completedPeekCount = 3;
    coverLifecycle.phaseTimer = 9.0f;
    NpcCoverLifecycle::Assign(
        coverLifecycle,
        91,
        false,
        { 1.0f, 2.0f, 3.0f },
        { 2.0f, 2.0f, 3.0f },
        true,
        { 0.0f, 2.0f, 0.0f },
        true);
    Expect(coverLifecycle.reservedCover == 91 &&
        coverLifecycle.debugForcedCover &&
        coverLifecycle.debugForcedCoverAction &&
        coverLifecycle.failedPeekCount == 0 &&
        coverLifecycle.completedPeekCount == 0 &&
        coverLifecycle.phaseTimer == 0.0f,
        "automatic and debug cover assignment share one clean lifecycle reset");
    NpcCoverLifecycle::Clear(coverLifecycle);
    Expect(coverLifecycle.reservedCover == INVALID_ENTITY &&
        !coverLifecycle.dynamicCoverActive &&
        !coverLifecycle.debugForcedCover &&
        !coverLifecycle.debugForcedCoverAction,
        "clearing a forced cover removes both ownership and action markers");

    Expect(NpcCombatMath::IsInsideViewCone(0.0f, 0.0f, -5.0f, 120.0f),
        "combat math accepts a target in front of the NPC");
    Expect(!NpcCombatMath::IsInsideViewCone(0.0f, 0.0f, 5.0f, 120.0f),
        "combat math rejects a target behind the ordinary view cone");
    Expect(NpcCombatMath::IsInsideViewCone(0.0f, 0.0f, 5.0f, 360.0f),
        "a full view cone accepts a target behind the NPC");
    Expect(!NpcCombatMath::IsInsideViewCone(
            0.0f,
            (std::numeric_limits<float>::infinity)(),
            0.0f,
            120.0f),
        "combat view math rejects non-finite directions");
    Expect(!NpcCombatMath::IsWithinDistanceSquared(
            (std::numeric_limits<float>::infinity)(), 10.0f),
        "combat distance math rejects non-finite input");
    Expect(!NpcCombatMath::IsWithinDistanceSquared(-1.0f, 10.0f),
        "combat distance math rejects a negative squared distance");
    float unchangedYaw = 1.25f;
    Expect(!NpcCombatMath::TryTargetYaw(0.0f, 0.0f, unchangedYaw) &&
        unchangedYaw == 1.25f,
        "zero target direction does not overwrite the caller's yaw");
    const float wrappedStep = NpcCombatMath::StepYawToward(
        DirectX::XM_PI - 0.05f,
        -DirectX::XM_PI + 0.05f,
        0.025f);
    Expect(std::fabs(NpcCombatMath::ShortestYawDelta(
            wrappedStep,
            -DirectX::XM_PI + 0.05f)) <= 0.076f,
        "yaw stepping follows the short path across the pi boundary");

    NpcSimulationStudio::ClearHistory();
    NpcDebugCombatActionRequest queuedAction;
    queuedAction.npc = 42;
    queuedAction.action = NpcDebugCombatAction::DodgeLeft;
    NpcSimulationStudio::QueueCombatAction(queuedAction);
    NpcDebugCombatActionRequest consumedAction;
    Expect(NpcSimulationStudio::ConsumeCombatAction(consumedAction) &&
        consumedAction.npc == 42 &&
        consumedAction.action == NpcDebugCombatAction::DodgeLeft,
        "a Combat Lab action request is consumed with its original payload");
    Expect(!NpcSimulationStudio::ConsumeCombatAction(consumedAction),
        "a Combat Lab action request is consumed exactly once");
    const auto dodgeLeft = NpcCombatMath::HorizontalLeft(
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, -1.0f });
    Expect(dodgeLeft.x < 0.0f && std::fabs(dodgeLeft.z) < 0.0001f,
        "left dodge direction is perpendicular to a north-facing threat");
    Expect(NpcCombatDecisionRules::HasCloseProximityAwareness(0.01f, 2.0f),
        "a nearby enemy is noticed even outside the ordinary view cone");
    Expect(!NpcCombatDecisionRules::HasCloseProximityAwareness(9.0f, 2.0f),
        "proximity awareness does not become long-range omnidirectional sight");
    Expect(!NpcCombatDecisionRules::ShouldFaceCombatAttention(
            NpcState::Chasing, true, false),
        "a long combat run faces its route instead of backpedaling");
    Expect(NpcCombatDecisionRules::ShouldFaceCombatAttention(
            NpcState::TakingCover, true, true),
        "the short peek step may keep attention on the threat");
    Expect(NpcCombatDecisionRules::CanUsePeekPose(
            true, true, false, NpcState::Attacking),
        "a stationary exposed attacker may use the authored peek pose");
    Expect(!NpcCombatDecisionRules::CanUsePeekPose(
            true, true, true, NpcState::Chasing),
        "a stale peek flag cannot override locomotion during repositioning");
    Expect(NpcCombatDecisionRules::ShouldRunDuringCombat(
            NpcState::TakingCover, false, false, 0.5f, 4.0f),
        "returning to protected cover uses an emergency run at short range");
    Expect(!NpcCombatDecisionRules::ShouldRunDuringCombat(
            NpcState::TakingCover, true, true, 0.5f, 4.0f),
        "the short peek motion is not replaced by a run animation");
    Expect(NpcCombatDecisionRules::ShouldRunDuringCombat(
            NpcState::Chasing, false, true, 0.5f, 4.0f),
        "incoming fire makes a combat chase urgent");
    Expect(!NpcCombatDecisionRules::ShouldRunDuringCombat(
            NpcState::Chasing, false, false, 2.0f, 4.0f),
        "a nearby combat adjustment remains a controlled walk");
    Expect(NpcCombatDecisionRules::ShouldHoldCoverForWeapon(true, 12, 60),
        "an active reload blocks the next peek");
    Expect(NpcCombatDecisionRules::ShouldHoldCoverForWeapon(false, 0, 60),
        "an empty magazine blocks a peek before the reload begins");
    Expect(NpcCombatDecisionRules::ShouldHoldCoverForWeapon(false, 0, 0),
        "an NPC without ammunition remains behind protection");
    Expect(NpcCombatDecisionRules::ShouldRelocateAfterFailedPeeks(2, 2),
        "repeated exposed hits compromise the current cover");
    Expect(NpcCombatDecisionRules::PeekCyclesBeforeReposition(
            NpcTacticalStyle::Assault) == 1,
        "assault changes firing angle after one completed peek");
    Expect(NpcCombatDecisionRules::PeekCyclesBeforeReposition(
            NpcTacticalStyle::Defensive) == 3,
        "defensive style may reuse reliable cover longer than assault");
    Expect(NpcCombatDecisionRules::ShouldRepositionAfterPeekCycle(
            NpcTacticalStyle::Balanced, 2, 0, 4),
        "safe repeated peeks eventually request a new firing angle");
    Expect(!NpcCombatDecisionRules::ShouldRepositionAfterPeekCycle(
            NpcTacticalStyle::Defensive, 2, 0, 4),
        "defensive NPC does not abandon cover after a single safe cycle");
    Expect(NpcCombatDecisionRules::CanReactToIncomingFireDirection(
            false, 20.0f * 20.0f, 25.0f),
        "a nearby ordinary hit reveals only its incoming direction");
    Expect(!NpcCombatDecisionRules::CanReactToIncomingFireDirection(
            false, 500.0f * 500.0f, 25.0f),
        "a distant hit does not provide an exact turn direction");
    Expect(!NpcCombatDecisionRules::CanReactToIncomingFireDirection(
            true, 2.0f * 2.0f, 25.0f),
        "a silent kill never triggers the incoming-fire reaction");
    Expect(NpcCombatDecisionRules::CanFinishFlank(true, true, false, true),
        "flank ends early after acquiring a separated usable firing position");
    Expect(!NpcCombatDecisionRules::CanFinishFlank(true, false, false, true),
        "seeing target without a usable shot does not finish flank");
    Expect(!NpcCombatDecisionRules::CanFinishFlank(true, true, true, true),
        "incoming fire does not force NPC to stop in the open");
    Expect(!NpcCombatDecisionRules::CanFinishFlank(true, true, false, false),
        "flanker does not settle against its teammate");
    if (failures != 0)
    {
        std::cerr << failures << " contract test(s) failed.\n";
        return 1;
    }

    std::cout << "All core contract tests passed.\n";
    return 0;
}
