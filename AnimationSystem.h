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

        for (auto& [entity, animComp] : animPool)
        {
            if (!animComp.isPlaying) continue;

            auto* mesh = meshPool.Get(entity);
            if (!mesh || !mesh->model || !mesh->model->HasAnimations()) continue;

            mesh->model->UpdateAnimation(animComp, deltaTime);
        }
    }
};
