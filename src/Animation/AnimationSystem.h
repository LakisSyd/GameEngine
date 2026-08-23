#pragma once
#include "ECSRegistry.h"
#include "ECSTypes.h"
#include "assimpmodelclass.h"
#include "AnimationComponent.h"

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

            mesh->model->UpdateAnimation(animComp, deltaTime);
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

            animComp.currentState = sourceAnim->currentState;
            animComp.currentTime = sourceAnim->currentTime;
            animComp.isPlaying = sourceAnim->isPlaying;

            const auto* sourceClip = sourceMesh->model->GetAnimation(sourceAnim->animationIndex);
            if (sourceClip)
            {
                const int matchingClip =
                    targetMesh->model->FindAnimationIndexByName(sourceClip->name);
                if (matchingClip >= 0)
                    animComp.animationIndex = matchingClip;
            }

            if (animComp.isPlaying)
                targetMesh->model->UpdateAnimation(animComp, 0.0f);
        }
    }
};
