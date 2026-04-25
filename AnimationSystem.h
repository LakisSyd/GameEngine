#pragma once
#include "ECSRegistry .h"
#include "ECSTypes.h"
#include "assimpmodelclass.h"
#include "AnimationComponent.h"


class AnimationSystem {
public:
    // Обновляет все анимации в сцене
    // Вызывать из SceneManager::UpdateAll(float deltaTime)
    static void Update(ECSRegistry& registry, float deltaTime)
    {
        auto& animPool = registry.GetComponents<AnimationComponent>();
        auto& meshPool = registry.GetComponents<MeshRendererComponent>();

        for (auto& [entity, animComp] : animPool)
        {
            if (!animComp.isPlaying) continue;

            auto* mesh = meshPool.Get(entity);
            if (!mesh || !mesh->model || !mesh->model->HasAnimations()) continue;

            // 🔥 Управление временем — в системе
            const AssimpModelClass::AnimationClip* anim = mesh->model->GetAnimation(animComp.animationIndex);
            if (!anim || anim->duration <= 0.001f) continue;

            float ticksPerSecond = anim->ticksPerSecond > 0.0f ? anim->ticksPerSecond : 24.0f;
            animComp.currentTime += deltaTime;

            // Зацикливание
            if (animComp.currentTime >= anim->duration / ticksPerSecond)
                animComp.currentTime = 0.0f;

            // 🔥 Вычисление матриц — в модели (чистая функция)
            mesh->model->ComputeBoneMatrices(
                animComp.animationIndex,
                animComp.currentTime,
                animComp.finalBoneMatrices  // ← выходной буфер
            );
            mesh->model->UpdateAnimation(animComp, deltaTime);
        }
    }
};