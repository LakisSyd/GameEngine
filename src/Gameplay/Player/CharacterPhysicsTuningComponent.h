#pragma once

// CharacterPhysicsTuningComponent contains the advanced, editable part of the
// Bullet character-controller setup. It is intentionally separate from
// CharacterController: removing this header/component returns the project to
// the basic controller without coupling these knobs to Player or NPC logic.
//
// The same component is used by the player and NPCs. That matters because a
// ledge is a collision problem, not an AI problem; both characters must obey
// the same capsule and stepping rules.
struct CharacterPhysicsTuningComponent
{
    // Bullet stores the capsule by its CENTER, while TransformComponent stores
    // the point under the character's feet. With automatic offset enabled the
    // correct center is: cylindricalHeight / 2 + radius + footClearance.
    bool autoCalculateCapsuleCenterOffset = true;
    float manualCapsuleCenterOffsetY = 1.10f;
    float capsuleFootClearance = 0.02f;

    // Runtime Bullet options. Y-up is the convention used by this project.
    // World sweep (false) usually detects approaching walls more reliably than
    // querying only the overlap pairs already cached by the ghost object.
    bool forceYAxis = true;
    bool useGhostSweepTest = false;
    float maxPenetrationDepth = 0.04f;
    float fallSpeed = 30.0f;

    // Normal step-height safety limits. Disable the clamp when deliberately
    // experimenting, but remember that a very large step can hit a low ceiling
    // every frame and can help a character climb walls or reach the ship roof.
    bool clampNormalStepHeight = true;
    float minimumStepHeight = 0.05f;
    float maximumStepHeight = 0.45f;

    // Old scenes used 1.0 as stepHeight. On first runtime setup it is replaced
    // by the safer value below. Disable migration if 1.0 is intentional.
    bool migrateLegacyStepHeight = true;
    float legacyStepHeight = 1.0f;
    float legacyReplacementStepHeight = 0.35f;

    // Grounded step solver. The project does not rely only on Bullet's built-in
    // stepHeight anymore. After the character stops making progress, the solver
    // sweeps the REAL capsule upward, forward and then downward. A warp is
    // accepted only if there is headroom and the destination is a walkable floor
    // no more than automaticStepHeight above the current floor.
    bool automaticStepEnabled = true;
    // Predictive mode probes the next movement slice before the capsule actually
    // stops against the ledge. Keep this enabled for normal gameplay. Disabling
    // it restores the older diagnostic mode that waits for blocked movement.
    bool automaticStepPredictive = true;
    float automaticStepHeight = 0.30f;
    float automaticStepDelay = 0.06f;
    float automaticStepDuration = 0.12f;
    float automaticStepCooldown = 0.10f;
    float automaticStepProgressDistance = 0.020f;
    float automaticStepForwardProbe = 0.14f;
    float automaticStepDownProbe = 0.08f;
    float automaticStepSkin = 0.015f;
};
