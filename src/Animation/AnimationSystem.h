#pragma once
#include <algorithm>
#include "ECSRegistry.h"
#include "ECSTypes.h"
#include "assimpmodelclass.h"
#include "AnimationComponent.h"
#include "Gameplay/Player/PlayerComponents.h"

class AnimationSystem {
public:
    // Обновляет все анимации в сцене.
    // Важно: время анимации накапливается только в AssimpModelClass::UpdateAnimation,
    // чтобы не ускорять клипы двойным deltaTime.
    static void Update(ECSRegistry& registry, float deltaTime)
    {
        auto& animPool = registry.GetComponents<AnimationComponent>();
        auto& meshPool = registry.GetComponents<MeshRendererComponent>();

        // World characters own animation time. First-person copies are sampled
        // in a second pass so iteration order cannot put them one frame ahead.
        for (auto& [entity, animComp] : animPool)
        {
            if (auto* firstPerson = registry.GetComponent<FirstPersonViewComponent>(entity))
            {
                if (firstPerson->syncAnimation)
                    continue;
            }

            if (!animComp.isPlaying) continue;

            auto* mesh = meshPool.Get(entity);
            if (!mesh || !mesh->model || !mesh->model->HasAnimations()) continue;

            if (animComp.socketPreviewEnabled && !animComp.socketPreviewPunch)
            {
                if (mesh->model->GetAnimation(animComp.socketPreviewClip))
                    animComp.animationIndex = animComp.socketPreviewClip;
                animComp.upperBodyAnimationIndex =
                    animComp.socketPreviewBlendEnabled &&
                    mesh->model->GetAnimation(animComp.socketPreviewUpperClip)
                    ? animComp.socketPreviewUpperClip : -1;
                animComp.poseVariant = -1;
            }
            // При паузе сэмплируем точное время, заданное ползунком редактора.
            if (animComp.socketPreviewEnabled && !animComp.socketPreviewPunch &&
                !animComp.socketPreviewPlaying)
            {
                animComp.currentTime = animComp.socketPreviewTime;
                if (animComp.socketPreviewBlendEnabled)
                    animComp.upperBodyTime = animComp.socketPreviewUpperTime;
            }
            mesh->model->UpdateAnimation(animComp,
                animComp.socketPreviewEnabled && !animComp.socketPreviewPlaying
                    ? 0.0f : deltaTime);
            if (animComp.socketPreviewEnabled && animComp.socketPreviewPlaying)
            {
                if (!animComp.socketPreviewPunch &&
                    animComp.socketPreviewBlendEnabled &&
                    animComp.socketPreviewSyncLayerPhase)
                {
                    const auto* base = mesh->model->GetAnimation(
                        animComp.animationIndex);
                    const auto* upper = mesh->model->GetAnimation(
                        animComp.upperBodyAnimationIndex);
                    if (base && upper)
                    {
                        const float baseRate = base->ticksPerSecond > 0.0f
                            ? base->ticksPerSecond : 24.0f;
                        const float upperRate = upper->ticksPerSecond > 0.0f
                            ? upper->ticksPerSecond : 24.0f;
                        const float baseDuration = base->duration / baseRate;
                        const float upperDuration = upper->duration / upperRate;
                        if (baseDuration > 0.00001f && upperDuration > 0.00001f)
                        {
                            animComp.upperBodyTime =
                                animComp.currentTime / baseDuration * upperDuration;
                            mesh->model->UpdateAnimation(animComp, 0.0f);
                        }
                    }
                }
                animComp.socketPreviewTime = animComp.socketPreviewPunch
                    ? animComp.upperBodyTime : animComp.currentTime;
                if (!animComp.socketPreviewPunch &&
                    animComp.socketPreviewBlendEnabled)
                    animComp.socketPreviewUpperTime = animComp.upperBodyTime;
            }
        }

        for (auto& [entity, animComp] : animPool)
        {
            auto* firstPerson = registry.GetComponent<FirstPersonViewComponent>(entity);
            if (!firstPerson || !firstPerson->syncAnimation)
                continue;

            auto* targetMesh = meshPool.Get(entity);
            auto* sourceAnim = registry.GetComponent<AnimationComponent>(firstPerson->owner);
            auto* sourceMesh = meshPool.Get(firstPerson->owner);
            if (!targetMesh || !targetMesh->model || !sourceAnim ||
                !sourceMesh || !sourceMesh->model)
            {
                continue;
            }
            // FP preview uses the same per-clip speed settings. Gameplay FP
            // samples the world clock so punches and guard stay synchronized.
            animComp.clipSpeeds = sourceAnim->clipSpeeds;

            if (sourceAnim->socketPreviewEnabled && animComp.socketPreviewEnabled &&
                sourceAnim->socketPreviewPunch)
            {
                // Пробный удар использует игровой слой и фазу, а FP-клип
                // выбирается из того же варианта, что в реальном ударе.
                const int variant = sourceAnim->socketPreviewPunchVariant;
                const auto* sourceBase = sourceMesh->model->GetAnimation(sourceAnim->animationIndex);
                if (sourceBase)
                {
                    const int base = targetMesh->model->FindAnimationIndexByName(sourceBase->name);
                    if (base >= 0) animComp.animationIndex = base;
                }
                std::string fpName;
                if (variant >= 0 && variant < static_cast<int>(sourceAnim->punchVariants.size()))
                    fpName = sourceAnim->punchVariants[variant].firstPersonClip;
                if (fpName.empty())
                    for (const auto& binding : animComp.animations)
                        if (binding.state == AnimationState::Punch)
                        {
                            fpName = binding.name;
                            break;
                        }
                animComp.upperBodyAnimationIndex =
                    targetMesh->model->FindAnimationIndexBySemanticName(fpName);
                if (animComp.upperBodyAnimationIndex < 0)
                    for (const auto& binding : animComp.animations)
                        if (binding.state == AnimationState::Punch)
                        {
                            animComp.upperBodyAnimationIndex =
                                targetMesh->model->FindAnimationIndexBySemanticName(binding.name);
                            break;
                        }
                animComp.poseVariant = variant;
                animComp.upperBodyRoot = sourceAnim->upperBodyRoot;
                animComp.currentTime = sourceAnim->currentTime;
                animComp.isPlaying = true;
                const auto* worldClip = sourceMesh->model->GetAnimation(
                    sourceAnim->upperBodyAnimationIndex);
                const auto* fpClip = targetMesh->model->GetAnimation(
                    animComp.upperBodyAnimationIndex);
                if (worldClip && fpClip)
                {
                    const float worldRate = worldClip->ticksPerSecond > 0.0f
                        ? worldClip->ticksPerSecond : 24.0f;
                    const float fpRate = fpClip->ticksPerSecond > 0.0f
                        ? fpClip->ticksPerSecond : 24.0f;
                    const float worldDuration = worldClip->duration / worldRate;
                    const float fpDuration = fpClip->duration / fpRate;
                    const float phase = worldDuration > 0.0f
                        ? sourceAnim->socketPreviewTime / worldDuration : 0.0f;
                    animComp.upperBodyTime = fpDuration * std::clamp(phase, 0.0f, 0.999f);
                    animComp.socketPreviewTime = animComp.upperBodyTime;
                }
                targetMesh->model->UpdateAnimation(animComp, 0.0f);
                continue;
            }

            if (sourceAnim->socketPreviewEnabled && animComp.socketPreviewEnabled &&
                sourceAnim->socketPreviewFirstPersonFromWorld)
            {
                // Most FP authoring reuses the 3P action and only changes the
                // camera anchor, whole-arms framing and optional FP pose keys.
                // Match by clip name, copy the world playhead phase, then let
                // the FP model sample its own copy of that animation.
                const auto* worldClip = sourceMesh->model->GetAnimation(
                    sourceAnim->socketPreviewClip);
                int fpIndex = worldClip
                    ? targetMesh->model->FindAnimationIndexByName(worldClip->name)
                    : -1;
                if (fpIndex < 0 && worldClip)
                    fpIndex = targetMesh->model->FindAnimationIndexBySemanticName(
                        worldClip->name);
                const auto* fpClip = targetMesh->model->GetAnimation(fpIndex);
                if (worldClip && fpClip)
                {
                    const float worldRate = worldClip->ticksPerSecond > 0.0f
                        ? worldClip->ticksPerSecond : 24.0f;
                    const float fpRate = fpClip->ticksPerSecond > 0.0f
                        ? fpClip->ticksPerSecond : 24.0f;
                    const float worldDuration = worldClip->duration / worldRate;
                    const float fpDuration = fpClip->duration / fpRate;
                    const float phase = worldDuration > 0.00001f
                        ? sourceAnim->socketPreviewTime / worldDuration : 0.0f;
                    animComp.animationIndex = fpIndex;
                    animComp.upperBodyAnimationIndex = -1;
                    animComp.poseVariant = -1;
                    animComp.isPlaying = true;
                    animComp.socketPreviewClip = fpIndex;
                    animComp.socketPreviewPlaying = sourceAnim->socketPreviewPlaying;
                    animComp.socketPreviewTime = fpDuration *
                        std::clamp(phase, 0.0f, 0.999999f);
                    animComp.currentTime = animComp.socketPreviewTime;
                    targetMesh->model->UpdateAnimation(animComp, 0.0f);
                }
                continue;
            }

            if (sourceAnim->socketPreviewEnabled && animComp.socketPreviewEnabled)
            {
                // В студии руки от первого лица имеют собственный клип и
                // таймлайн: их можно править независимо от мировой модели.
                if (targetMesh->model->GetAnimation(animComp.socketPreviewClip))
                    animComp.animationIndex = animComp.socketPreviewClip;
                animComp.upperBodyAnimationIndex =
                    animComp.socketPreviewBlendEnabled &&
                    targetMesh->model->GetAnimation(animComp.socketPreviewUpperClip)
                    ? animComp.socketPreviewUpperClip : -1;
                animComp.poseVariant = -1;
                animComp.isPlaying = true;
                if (!animComp.socketPreviewPlaying)
                {
                    animComp.currentTime = animComp.socketPreviewTime;
                    if (animComp.socketPreviewBlendEnabled)
                        animComp.upperBodyTime = animComp.socketPreviewUpperTime;
                }
                targetMesh->model->UpdateAnimation(animComp,
                    animComp.socketPreviewPlaying ? deltaTime : 0.0f);
                if (animComp.socketPreviewPlaying)
                {
                    if (animComp.socketPreviewBlendEnabled &&
                        animComp.socketPreviewSyncLayerPhase)
                    {
                        const auto* base = targetMesh->model->GetAnimation(
                            animComp.animationIndex);
                        const auto* upper = targetMesh->model->GetAnimation(
                            animComp.upperBodyAnimationIndex);
                        if (base && upper)
                        {
                            const float baseRate = base->ticksPerSecond > 0.0f
                                ? base->ticksPerSecond : 24.0f;
                            const float upperRate = upper->ticksPerSecond > 0.0f
                                ? upper->ticksPerSecond : 24.0f;
                            const float baseDuration = base->duration / baseRate;
                            const float upperDuration = upper->duration / upperRate;
                            if (baseDuration > 0.00001f && upperDuration > 0.00001f)
                            {
                                animComp.upperBodyTime =
                                    animComp.currentTime / baseDuration * upperDuration;
                                targetMesh->model->UpdateAnimation(animComp, 0.0f);
                            }
                        }
                    }
                    animComp.socketPreviewTime = animComp.currentTime;
                    if (animComp.socketPreviewBlendEnabled)
                        animComp.socketPreviewUpperTime = animComp.upperBodyTime;
                }
                continue;
            }

            animComp.currentState = sourceAnim->currentState;
            animComp.currentTime = sourceAnim->currentTime;
            animComp.isPlaying = sourceAnim->isPlaying;
            animComp.upperBodyTime = sourceAnim->upperBodyTime;
            animComp.upperBodyRoot = sourceAnim->upperBodyRoot;
            animComp.poseVariant = -1;
            const auto* upperClip = sourceMesh->model->GetAnimation(sourceAnim->upperBodyAnimationIndex);
            animComp.upperBodyAnimationIndex = upperClip
                ? targetMesh->model->FindAnimationIndexByName(upperClip->name) : -1;

            if (const auto* player = registry.GetComponent<PlayerComponent>(firstPerson->owner);
                player && player->punchActive)
            {
                // У FP-рук свой удар. Совпадает только фаза удара, а клип и
                // поправки костей берутся из их собственного компонента.
                std::string fpName;
                if (player->punchVariant >= 0 &&
                    player->punchVariant < static_cast<int>(sourceAnim->punchVariants.size()))
                    fpName = sourceAnim->punchVariants[player->punchVariant].firstPersonClip;
                if (fpName.empty())
                    for (const auto& binding : animComp.animations)
                        if (binding.state == AnimationState::Punch)
                        {
                            fpName = binding.name;
                            break;
                        }
                int fpPunch = targetMesh->model->FindAnimationIndexBySemanticName(fpName);
                if (fpPunch < 0)
                    for (const auto& binding : animComp.animations)
                        if (binding.state == AnimationState::Punch)
                        {
                            fpPunch = targetMesh->model->FindAnimationIndexBySemanticName(binding.name);
                            break;
                        }
                if (fpPunch >= 0)
                    animComp.upperBodyAnimationIndex = fpPunch;
                animComp.poseVariant = player->punchVariant;
                if (const auto* clip = targetMesh->model->GetAnimation(
                    animComp.upperBodyAnimationIndex))
                {
                    const float rate = clip->ticksPerSecond > 0.0f
                        ? clip->ticksPerSecond : 24.0f;
                    const float duration = clip->duration / rate;
                    const float phase = player->punchDuration > 0.0f
                        ? player->punchTime / player->punchDuration : 0.0f;
                    animComp.upperBodyTime = duration * std::clamp(phase, 0.0f, 0.999f);
                }
            }

            const auto* sourceClip = sourceMesh->model->GetAnimation(sourceAnim->animationIndex);
            if (sourceClip)
            {
                const int matchingClip =
                    targetMesh->model->FindAnimationIndexByName(sourceClip->name);
                if (matchingClip >= 0)
                    animComp.animationIndex = matchingClip;
            }

            if (const auto* player = registry.GetComponent<PlayerComponent>(firstPerson->owner);
                player && player->meleeGuardActive)
            {
                const int guard = targetMesh->model->FindAnimationIndexBySemanticName(
                    sourceAnim->meleeActions.guardFirstPersonClip);
                if (guard >= 0)
                {
                    if (sourceAnim->currentState == AnimationState::Idle)
                        animComp.animationIndex = guard;
                    else if (sourceAnim->currentState == AnimationState::Walk)
                    {
                        const int movingGuard = targetMesh->model->FindAnimationIndexBySemanticName(
                            sourceAnim->meleeActions.guardMoveFirstPersonClip);
                        if (movingGuard >= 0)
                        {
                            animComp.animationIndex = movingGuard;
                            if (!player->punchActive)
                                animComp.upperBodyAnimationIndex = -1;
                        }
                        else if (!player->punchActive)
                            animComp.upperBodyAnimationIndex = guard;
                    }
                    else if (!player->punchActive)
                        animComp.upperBodyAnimationIndex = guard;
                }
            }

            if (animComp.isPlaying)
                targetMesh->model->UpdateAnimation(animComp, 0.0f);
        }
    }
};
