#pragma once

#include <atomic>
#include <cmath>
#include <memory>
#include <unordered_set>
#include <vector>

#include "AssetsLoadSystem.h"
#include "ECSRegistry.h"
#include "FrustumCulling.h"

// Готовит список мешей, потенциально видимых камерой. Worker получает только
// копии данных, а ECS остаётся полностью во владении главного потока.
class RenderCandidateSystem
{
public:
    static void Request(
        ECSRegistry& registry,
        AssetLoadSystem* assets,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection)
    {
        if (!assets || s_jobPending.exchange(true))
            return;

        auto snapshot = std::make_shared<Snapshot>();
        DirectX::XMStoreFloat4x4(&snapshot->view, view);
        DirectX::XMStoreFloat4x4(&snapshot->projection, projection);

        auto& meshPool = registry.GetComponents<MeshRendererComponent>();
        for (auto& [entity, mesh] : meshPool)
        {
            if (!registry.HasComponent<ActiveComponent>(entity) || !mesh.model)
                continue;
            if (registry.HasComponent<FirstPersonViewComponent>(entity))
                continue;

            auto* transform = registry.GetComponent<TransformComponent>(entity);
            if (!transform)
                continue;

            CandidateSnapshot candidate;
            candidate.entity = entity;
            DirectX::XMStoreFloat4x4(&candidate.world, transform->GetWorldMatrix());
            mesh.model->GetBoundingBox(&candidate.localMin, &candidate.localMax);
            snapshot->candidates.push_back(candidate);
        }

        if (!assets->GetJobSystem().SubmitJob([snapshot, assets]()
            {
                auto result = std::make_shared<Result>();
                result->view = snapshot->view;
                result->projection = snapshot->projection;

                const DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&snapshot->view);
                const DirectX::XMMATRIX projection = DirectX::XMLoadFloat4x4(&snapshot->projection);
                const FrustumCulling frustum(view, projection);

                for (const CandidateSnapshot& candidate : snapshot->candidates)
                {
                    // Запас уменьшает риск пропадания объекта на границе между кадрами.
                    if (frustum.IsVisible(
                        DirectX::XMLoadFloat4x4(&candidate.world),
                        candidate.localMin,
                        candidate.localMax,
                        2.0f))
                    {
                        result->visibleEntities.insert(candidate.entity);
                    }
                }

                assets->GetJobSystem().SubmitMainThread([result]()
                    {
                        s_cachedResult = std::move(result);
                        s_jobPending = false;
                    });
            }))
        {
            s_jobPending = false;
        }
    }

    static bool IsCandidate(
        Entity entity,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection)
    {
        if (!s_cachedResult ||
            !IsCloseToCachedMatrix(view, s_cachedResult->view) ||
            !IsCloseToCachedMatrix(projection, s_cachedResult->projection))
        {
            return true;
        }

        return s_cachedResult->visibleEntities.contains(entity);
    }

private:
    struct CandidateSnapshot
    {
        Entity entity = INVALID_ENTITY;
        DirectX::XMFLOAT4X4 world{};
        DirectX::XMFLOAT3 localMin{};
        DirectX::XMFLOAT3 localMax{};
    };

    struct Snapshot
    {
        DirectX::XMFLOAT4X4 view{};
        DirectX::XMFLOAT4X4 projection{};
        std::vector<CandidateSnapshot> candidates;
    };

    struct Result
    {
        DirectX::XMFLOAT4X4 view{};
        DirectX::XMFLOAT4X4 projection{};
        std::unordered_set<Entity> visibleEntities;
    };

    static bool IsCloseToCachedMatrix(
        const DirectX::XMMATRIX& matrix,
        const DirectX::XMFLOAT4X4& cached)
    {
        DirectX::XMFLOAT4X4 current{};
        DirectX::XMStoreFloat4x4(&current, matrix);

        constexpr float tolerance = 0.02f;
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                if (std::abs(current.m[row][column] - cached.m[row][column]) > tolerance)
                    return false;
            }
        }

        return true;
    }

    inline static std::atomic_bool s_jobPending = false;
    inline static std::shared_ptr<Result> s_cachedResult;
};
