#pragma once

#include <cstdint>
#include <d3d11.h>

#include "cameraclass.h"
#include "ECSRegistry.h"
#include "../Components/Components.h"
#include "JobSystem.h"

namespace MeshCuttingTest
{
    // Prototype only: removes triangles around a random point from the reactor area.
    // This is not final CSG/destruction. It is a cheap runtime mesh mutation test.
    bool CutRandomHoleInReactor(ECSRegistry& registry, ID3D11Device* device);

    // Casts a ray into scene meshes and cuts the closest triangle hit.
    bool CutByRay(ECSRegistry& registry, ID3D11Device* device, const CameraClass::Ray& ray);

    // Same ray cutter, but the heavy triangle clipping runs on JobSystem workers.
    // DirectX buffers, ECS and Bullet collision are still updated on the main thread.
    bool CutByRayAsync(
        ECSRegistry& registry,
        ID3D11Device* device,
        JobSystem& jobs,
        const CameraClass::Ray& ray,
        float radiusOverride = 0.0f,
        bool cutThrough = false,
        std::uint64_t* operationIdOut = nullptr);

    // Удаляет один логический вырез из всех связанных мешей и заново
    // проигрывает оставшиеся повреждения от исходной геометрии.
    bool RepairCut(ECSRegistry& registry, std::uint64_t operationId);

    // Async equivalent used by gameplay repair. CPU reconstruction runs on a
    // worker; DirectX buffers, ECS history and Bullet bodies are applied on the
    // main thread. Returns true only after the main-thread apply has completed.
    bool RepairCutAsync(
        ECSRegistry& registry,
        JobSystem& jobs,
        std::uint64_t operationId);
}
