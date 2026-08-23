#include "MeshCuttingTest.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <DirectXCollision.h>

#include "PhysicsSystem.h"
#include "Gameplay/Repair/RuntimeMeshDamageComponent.h"

namespace
{
    std::atomic<std::uint64_t> g_nextCutOperationId{ 1 };

    struct Vec2
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct PolyVertex
    {
        AssimpModelClass::VertexType vertex{};
        Vec2 disk{};
    };

    struct RayModelHit
    {
        Entity entity = INVALID_ENTITY;
        AssimpModelClass* model = nullptr;
        DirectX::XMFLOAT3 localPosition{};
        DirectX::XMFLOAT3 localNormal{};
        DirectX::XMFLOAT3 localRayDirection{};
        float worldDistance = (std::numeric_limits<float>::max)();
    };

    bool ContainsNoCase(const std::string& text, const char* needle)
    {
        std::string upperText = text;
        for (char& c : upperText)
            c = static_cast<char>(::toupper(static_cast<unsigned char>(c)));

        return upperText.find(needle) != std::string::npos;
    }

    float Length(const DirectX::XMFLOAT3& v)
    {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    float DistanceSquared(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        const float dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    }

    DirectX::XMFLOAT3 Subtract(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
    {
        return { a.x - b.x, a.y - b.y, a.z - b.z };
    }

    DirectX::XMFLOAT3 NormalizeOrFallback(
        const DirectX::XMFLOAT3& value,
        const DirectX::XMFLOAT3& fallback)
    {
        DirectX::XMVECTOR v = DirectX::XMLoadFloat3(&value);
        const float lenSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(v));
        if (lenSq < 0.000001f)
            return fallback;

        v = DirectX::XMVector3Normalize(v);
        DirectX::XMFLOAT3 result{};
        DirectX::XMStoreFloat3(&result, v);
        return result;
    }

    DirectX::XMFLOAT3 Cross(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
    {
        DirectX::XMVECTOR c = DirectX::XMVector3Cross(
            DirectX::XMLoadFloat3(&a),
            DirectX::XMLoadFloat3(&b));

        DirectX::XMFLOAT3 result{};
        DirectX::XMStoreFloat3(&result, c);
        return result;
    }

    float Dot(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    AssimpModelClass::VertexType LerpVertex(
        const AssimpModelClass::VertexType& a,
        const AssimpModelClass::VertexType& b,
        float t)
    {
        AssimpModelClass::VertexType out{};

        auto lerp = [t](float x, float y) { return x + (y - x) * t; };

        out.position = {
            lerp(a.position.x, b.position.x),
            lerp(a.position.y, b.position.y),
            lerp(a.position.z, b.position.z)
        };
        out.texture = {
            lerp(a.texture.x, b.texture.x),
            lerp(a.texture.y, b.texture.y)
        };
        out.normal = NormalizeOrFallback({
            lerp(a.normal.x, b.normal.x),
            lerp(a.normal.y, b.normal.y),
            lerp(a.normal.z, b.normal.z)
        }, { 0.0f, 1.0f, 0.0f });
        out.tangent = NormalizeOrFallback({
            lerp(a.tangent.x, b.tangent.x),
            lerp(a.tangent.y, b.tangent.y),
            lerp(a.tangent.z, b.tangent.z)
        }, { 1.0f, 0.0f, 0.0f });
        out.binormal = NormalizeOrFallback({
            lerp(a.binormal.x, b.binormal.x),
            lerp(a.binormal.y, b.binormal.y),
            lerp(a.binormal.z, b.binormal.z)
        }, { 0.0f, 0.0f, 1.0f });

        for (int i = 0; i < 4; ++i)
        {
            out.boneIDs[i] = a.boneIDs[i];
            out.boneWeights[i] = lerp(a.boneWeights[i], b.boneWeights[i]);
        }

        return out;
    }

    PolyVertex LerpPolyVertex(const PolyVertex& a, const PolyVertex& b, float t)
    {
        PolyVertex out{};
        out.vertex = LerpVertex(a.vertex, b.vertex, t);
        out.disk = {
            a.disk.x + (b.disk.x - a.disk.x) * t,
            a.disk.y + (b.disk.y - a.disk.y) * t
        };
        return out;
    }

    float SignedDistanceToEdge(const Vec2& p, const Vec2& a, const Vec2& b)
    {
        const float edgeX = b.x - a.x;
        const float edgeY = b.y - a.y;
        const float pointX = p.x - a.x;
        const float pointY = p.y - a.y;
        return edgeX * pointY - edgeY * pointX;
    }

    float DistancePointSegmentSq(const Vec2& p, const Vec2& a, const Vec2& b)
    {
        const float abX = b.x - a.x;
        const float abY = b.y - a.y;
        const float apX = p.x - a.x;
        const float apY = p.y - a.y;
        const float abLenSq = abX * abX + abY * abY;

        float t = 0.0f;
        if (abLenSq > 0.000001f)
            t = (apX * abX + apY * abY) / abLenSq;

        if (t < 0.0f)
            t = 0.0f;
        else if (t > 1.0f)
            t = 1.0f;

        const float closestX = a.x + abX * t;
        const float closestY = a.y + abY * t;
        const float dx = p.x - closestX;
        const float dy = p.y - closestY;
        return dx * dx + dy * dy;
    }

    bool PointInTriangle2D(const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c)
    {
        const float d0 = SignedDistanceToEdge(p, a, b);
        const float d1 = SignedDistanceToEdge(p, b, c);
        const float d2 = SignedDistanceToEdge(p, c, a);

        const bool hasNegative = d0 < 0.0f || d1 < 0.0f || d2 < 0.0f;
        const bool hasPositive = d0 > 0.0f || d1 > 0.0f || d2 > 0.0f;
        return !(hasNegative && hasPositive);
    }

    bool CircleOverlapsTriangle2D(const Vec2& a, const Vec2& b, const Vec2& c, float radius)
    {
        const float radiusSq = radius * radius;
        const Vec2 center{ 0.0f, 0.0f };

        auto lenSq = [](const Vec2& p)
        {
            return p.x * p.x + p.y * p.y;
        };

        if (lenSq(a) <= radiusSq || lenSq(b) <= radiusSq || lenSq(c) <= radiusSq)
            return true;

        if (PointInTriangle2D(center, a, b, c))
            return true;

        return DistancePointSegmentSq(center, a, b) <= radiusSq ||
            DistancePointSegmentSq(center, b, c) <= radiusSq ||
            DistancePointSegmentSq(center, c, a) <= radiusSq;
    }

    std::vector<Vec2> BuildIrregularCutProfile(
        const DirectX::XMFLOAT3& localCenter,
        float radius,
        int segmentCount)
    {
        std::vector<Vec2> profile;
        profile.reserve(segmentCount);

        // Low-frequency harmonics keep the polygon convex enough for the clipping
        // routine while removing the artificial cookie-cutter circle silhouette.
        const float phase = std::sin(
            localCenter.x * 17.13f +
            localCenter.y * 31.71f +
            localCenter.z * 11.37f) * DirectX::XM_PI;

        for (int i = 0; i < segmentCount; ++i)
        {
            const float angle = DirectX::XM_2PI * static_cast<float>(i) /
                static_cast<float>(segmentCount);
            const float radialScale =
                1.0f +
                0.075f * std::sin(angle * 3.0f + phase) +
                0.035f * std::sin(angle * 2.0f - phase * 0.7f);
            const float irregularRadius = radius * radialScale;
            profile.push_back({
                std::cos(angle) * irregularRadius,
                std::sin(angle) * irregularRadius
            });
        }

        return profile;
    }

    bool RayIntersectsAabb(
        const DirectX::XMFLOAT3& origin,
        const DirectX::XMFLOAT3& direction,
        const DirectX::XMFLOAT3& min,
        const DirectX::XMFLOAT3& max,
        float maxDistance)
    {
        float tMin = 0.0f;
        float tMax = maxDistance;

        auto testAxis = [&](float o, float d, float axisMin, float axisMax)
        {
            constexpr float eps = 0.000001f;
            if (std::fabs(d) < eps)
                return o >= axisMin && o <= axisMax;

            float t1 = (axisMin - o) / d;
            float t2 = (axisMax - o) / d;
            if (t1 > t2)
                std::swap(t1, t2);

            if (t1 > tMin)
                tMin = t1;
            if (t2 < tMax)
                tMax = t2;

            return tMin <= tMax;
        };

        return testAxis(origin.x, direction.x, min.x, max.x) &&
            testAxis(origin.y, direction.y, min.y, max.y) &&
            testAxis(origin.z, direction.z, min.z, max.z) &&
            tMax >= 0.0f;
    }

    std::vector<PolyVertex> ClipPolygonByEdge(
        const std::vector<PolyVertex>& polygon,
        const Vec2& a,
        const Vec2& b,
        bool keepInside)
    {
        std::vector<PolyVertex> output;
        if (polygon.empty())
            return output;

        constexpr float eps = 0.00001f;
        output.reserve(polygon.size() + 2);

        for (size_t i = 0; i < polygon.size(); ++i)
        {
            const PolyVertex& current = polygon[i];
            const PolyVertex& previous = polygon[(i + polygon.size() - 1) % polygon.size()];

            const float currentDistance = SignedDistanceToEdge(current.disk, a, b);
            const float previousDistance = SignedDistanceToEdge(previous.disk, a, b);

            const bool currentInside = keepInside
                ? currentDistance >= -eps
                : currentDistance <= eps;
            const bool previousInside = keepInside
                ? previousDistance >= -eps
                : previousDistance <= eps;

            if (currentInside != previousInside)
            {
                const float denom = previousDistance - currentDistance;
                float t = 0.0f;
                if (std::fabs(denom) > eps)
                    t = previousDistance / denom;
                output.push_back(LerpPolyVertex(previous, current, t));
            }

            if (currentInside)
                output.push_back(current);
        }

        return output;
    }

    void AppendTriangulatedPolygon(
        const std::vector<PolyVertex>& polygon,
        std::vector<AssimpModelClass::VertexType>& outVertices,
        std::vector<unsigned long>& outIndices)
    {
        if (polygon.size() < 3)
            return;

        for (size_t i = 1; i + 1 < polygon.size(); ++i)
        {
            const unsigned long base = static_cast<unsigned long>(outVertices.size());
            outVertices.push_back(polygon[0].vertex);
            outVertices.push_back(polygon[i].vertex);
            outVertices.push_back(polygon[i + 1].vertex);
            outIndices.push_back(base);
            outIndices.push_back(base + 1);
            outIndices.push_back(base + 2);
        }
    }

    void AppendTriangle(
        const AssimpModelClass::VertexType& a,
        const AssimpModelClass::VertexType& b,
        const AssimpModelClass::VertexType& c,
        std::vector<AssimpModelClass::VertexType>& outVertices,
        std::vector<unsigned long>& outIndices)
    {
        const unsigned long base = static_cast<unsigned long>(outVertices.size());
        outVertices.push_back(a);
        outVertices.push_back(b);
        outVertices.push_back(c);
        outIndices.push_back(base);
        outIndices.push_back(base + 1);
        outIndices.push_back(base + 2);
    }

    bool SubtractConvexDiskFromTriangle(
        const AssimpModelClass::VertexType& a,
        const AssimpModelClass::VertexType& b,
        const AssimpModelClass::VertexType& c,
        const DirectX::XMFLOAT3& center,
        const DirectX::XMFLOAT3& axisU,
        const DirectX::XMFLOAT3& axisV,
        const std::vector<Vec2>& disk,
        std::vector<AssimpModelClass::VertexType>& outVertices,
        std::vector<unsigned long>& outIndices)
    {
        auto toDisk = [&](const DirectX::XMFLOAT3& p) -> Vec2
        {
            const DirectX::XMFLOAT3 relative = Subtract(p, center);
            return { Dot(relative, axisU), Dot(relative, axisV) };
        };

        std::vector<std::vector<PolyVertex>> insidePieces;
        insidePieces.push_back({
            { a, toDisk(a.position) },
            { b, toDisk(b.position) },
            { c, toDisk(c.position) }
        });

        std::vector<std::vector<PolyVertex>> outsidePieces;

        for (size_t edge = 0; edge < disk.size(); ++edge)
        {
            const Vec2 edgeA = disk[edge];
            const Vec2 edgeB = disk[(edge + 1) % disk.size()];

            std::vector<std::vector<PolyVertex>> nextInside;

            for (const auto& polygon : insidePieces)
            {
                std::vector<PolyVertex> outside = ClipPolygonByEdge(polygon, edgeA, edgeB, false);
                if (outside.size() >= 3)
                    outsidePieces.push_back(std::move(outside));

                std::vector<PolyVertex> inside = ClipPolygonByEdge(polygon, edgeA, edgeB, true);
                if (inside.size() >= 3)
                    nextInside.push_back(std::move(inside));
            }

            insidePieces = std::move(nextInside);
            if (insidePieces.empty())
                break;
        }

        if (outsidePieces.empty() && !insidePieces.empty())
        {
            // Entire triangle was inside the cutter, so it is removed.
            return true;
        }

        for (const auto& polygon : outsidePieces)
            AppendTriangulatedPolygon(polygon, outVertices, outIndices);

        return !outsidePieces.empty();
    }

    void RebuildCpuPositionCaches(
        const std::vector<AssimpModelClass::VertexType>& vertices,
        const std::vector<unsigned long>& indices,
        std::vector<DirectX::XMFLOAT3>& outVertices,
        std::vector<DirectX::XMFLOAT3>& outTriangles)
    {
        outVertices.clear();
        outTriangles.clear();
        outVertices.reserve(vertices.size());

        for (const auto& vertex : vertices)
            outVertices.push_back(vertex.position);
    }

    bool RaycastModel(
        Entity entity,
        AssimpModelClass* model,
        const TransformComponent& transform,
        const CameraClass::Ray& ray,
        RayModelHit& closest)
    {
        if (!model)
            return false;

        const DirectX::XMMATRIX world = transform.GetWorldMatrix();
        const DirectX::XMMATRIX inverseWorld = DirectX::XMMatrixInverse(nullptr, world);

        const DirectX::XMVECTOR worldOrigin = DirectX::XMLoadFloat3(&ray.origin);
        const DirectX::XMVECTOR localOrigin = DirectX::XMVector3TransformCoord(worldOrigin, inverseWorld);
        const DirectX::XMVECTOR localDirection = DirectX::XMVector3Normalize(
            DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&ray.direction), inverseWorld));

        DirectX::XMFLOAT3 localOriginFloat{};
        DirectX::XMFLOAT3 localDirectionFloat{};
        DirectX::XMStoreFloat3(&localOriginFloat, localOrigin);
        DirectX::XMStoreFloat3(&localDirectionFloat, localDirection);

        DirectX::XMFLOAT3 bbMin{};
        DirectX::XMFLOAT3 bbMax{};
        model->GetBoundingBox(&bbMin, &bbMax);
        if (!RayIntersectsAabb(localOriginFloat, localDirectionFloat, bbMin, bbMax, closest.worldDistance))
            return false;

        const auto& vertices = model->GetCpuMeshVerticesForTest();
        const auto& indices = model->GetCpuMeshIndicesForTest();
        if (vertices.empty() || indices.size() < 3)
            return false;

        bool hitAnything = false;
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            const unsigned long ia = indices[i];
            const unsigned long ib = indices[i + 1];
            const unsigned long ic = indices[i + 2];
            if (ia >= vertices.size() || ib >= vertices.size() || ic >= vertices.size())
                continue;

            const DirectX::XMFLOAT3& a = vertices[ia].position;
            const DirectX::XMFLOAT3& b = vertices[ib].position;
            const DirectX::XMFLOAT3& c = vertices[ic].position;

            float localDistance = 0.0f;
            if (!DirectX::TriangleTests::Intersects(
                localOrigin,
                localDirection,
                DirectX::XMLoadFloat3(&a),
                DirectX::XMLoadFloat3(&b),
                DirectX::XMLoadFloat3(&c),
                localDistance))
            {
                continue;
            }

            const DirectX::XMVECTOR localHit = DirectX::XMVectorAdd(
                localOrigin,
                DirectX::XMVectorScale(localDirection, localDistance));
            const DirectX::XMVECTOR worldHit = DirectX::XMVector3TransformCoord(localHit, world);
            const float worldDistance = DirectX::XMVectorGetX(DirectX::XMVector3Length(
                DirectX::XMVectorSubtract(worldHit, worldOrigin)));

            if (worldDistance >= closest.worldDistance)
                continue;

            DirectX::XMFLOAT3 localHitFloat{};
            DirectX::XMStoreFloat3(&localHitFloat, localHit);

            DirectX::XMFLOAT3 e0 = Subtract(b, a);
            DirectX::XMFLOAT3 e1 = Subtract(c, a);
            DirectX::XMFLOAT3 normal = NormalizeOrFallback(Cross(e0, e1), { 0.0f, 1.0f, 0.0f });

            if (Dot(normal, localDirectionFloat) > 0.0f)
                normal = { -normal.x, -normal.y, -normal.z };

            closest.entity = entity;
            closest.model = model;
            closest.localPosition = localHitFloat;
            closest.localNormal = normal;
            closest.localRayDirection = localDirectionFloat;
            closest.worldDistance = worldDistance;
            hitAnything = true;
        }

        return hitAnything;
    }

    float CalculateCutRadius(AssimpModelClass* model)
    {
        if (!model)
            return 0.25f;

        DirectX::XMFLOAT3 bbMin{};
        DirectX::XMFLOAT3 bbMax{};
        model->GetBoundingBox(&bbMin, &bbMax);
        const DirectX::XMFLOAT3 size{
            bbMax.x - bbMin.x,
            bbMax.y - bbMin.y,
            bbMax.z - bbMin.z
        };

        float radius = Length(size) * 0.055f;
        if (radius < 0.18f)
            radius = 0.18f;
        if (radius > 0.75f)
            radius = 0.75f;
        return radius;
    }

    float CalculateThroughCutDepth(
        AssimpModelClass* model,
        const DirectX::XMFLOAT3& localCenter,
        const DirectX::XMFLOAT3& localDirection,
        float radius)
    {
        if (!model)
            return radius;

        DirectX::XMFLOAT3 bbMin{};
        DirectX::XMFLOAT3 bbMax{};
        model->GetBoundingBox(&bbMin, &bbMax);

        const DirectX::XMFLOAT3 axis = NormalizeOrFallback(
            localDirection,
            { 0.0f, 0.0f, 1.0f });

        float depth = 0.0f;
        for (int x = 0; x < 2; ++x)
        {
            for (int y = 0; y < 2; ++y)
            {
                for (int z = 0; z < 2; ++z)
                {
                    const DirectX::XMFLOAT3 corner{
                        x == 0 ? bbMin.x : bbMax.x,
                        y == 0 ? bbMin.y : bbMax.y,
                        z == 0 ? bbMin.z : bbMax.z
                    };
                    depth = (std::max)(depth, std::fabs(Dot(Subtract(corner, localCenter), axis)));
                }
            }
        }

        // The margin catches triangles that lie exactly on the far model bound.
        return depth + (std::max)(radius * 0.1f, 0.01f);
    }

        float ConvertLocalDepthToWorld(
        const TransformComponent& transform,
        const DirectX::XMFLOAT3& localDirection,
        float localDepth)
    {
        const DirectX::XMFLOAT3 normalizedDirection = NormalizeOrFallback(
            localDirection,
            { 0.0f, 0.0f, 1.0f });
        const DirectX::XMVECTOR localDelta = DirectX::XMVectorScale(
            DirectX::XMLoadFloat3(&normalizedDirection),
            localDepth);
        const DirectX::XMVECTOR worldDelta = DirectX::XMVector3TransformNormal(
            localDelta,
            transform.GetWorldMatrix());
        return DirectX::XMVectorGetX(DirectX::XMVector3Length(worldDelta));
    }

    float ConvertWorldDepthToLocal(
        const TransformComponent& transform,
        const DirectX::XMFLOAT3& worldDirection,
        float worldDepth)
    {
        const DirectX::XMMATRIX inverseWorld = DirectX::XMMatrixInverse(
            nullptr,
            transform.GetWorldMatrix());
        const DirectX::XMFLOAT3 normalizedDirection = NormalizeOrFallback(
            worldDirection,
            { 0.0f, 0.0f, 1.0f });
        const DirectX::XMVECTOR worldDelta = DirectX::XMVectorScale(
            DirectX::XMLoadFloat3(&normalizedDirection),
            worldDepth);
        const DirectX::XMVECTOR localDelta = DirectX::XMVector3TransformNormal(worldDelta, inverseWorld);
        return DirectX::XMVectorGetX(DirectX::XMVector3Length(localDelta));
    }
struct AsyncCutResult
    {
        Entity entity = INVALID_ENTITY;
        AssimpModelClass* model = nullptr;
        std::vector<AssimpModelClass::VertexType> vertices;
        std::vector<unsigned long> indices;
        int changedTriangles = 0;
        int finalTriangles = 0;
        float radius = 0.0f;
        RuntimeMeshCutOperation operation{};
        bool success = false;
    };

    std::mutex g_pendingCutMutex;
    std::unordered_set<AssimpModelClass*> g_pendingCutModels;

    bool TryMarkCutPending(AssimpModelClass* model)
    {
        if (!model)
            return false;

        std::lock_guard<std::mutex> lock(g_pendingCutMutex);
        if (g_pendingCutModels.find(model) != g_pendingCutModels.end())
            return false;

        g_pendingCutModels.insert(model);
        return true;
    }

    void ClearCutPending(AssimpModelClass* model)
    {
        std::lock_guard<std::mutex> lock(g_pendingCutMutex);
        g_pendingCutModels.erase(model);
    }

    bool IsCutPending(AssimpModelClass* model)
    {
        std::lock_guard<std::mutex> lock(g_pendingCutMutex);
        return g_pendingCutModels.find(model) != g_pendingCutModels.end();
    }

    enum class AsyncRepairState
    {
        Pending,
        Completed,
        Failed
    };

    struct AsyncRepairInput
    {
        Entity entity = INVALID_ENTITY;
        AssimpModelClass* model = nullptr;
        ID3D11Device* device = nullptr;
        std::uint64_t expectedRevision = 0;
        std::shared_ptr<const RuntimeMeshGeometrySnapshot> baseline;
        std::vector<RuntimeMeshCutOperation> remainingCuts;
    };

    struct AsyncRepairTargetResult
    {
        Entity entity = INVALID_ENTITY;
        AssimpModelClass* model = nullptr;
        ID3D11Device* device = nullptr;
        std::uint64_t expectedRevision = 0;
        std::vector<RuntimeMeshCutOperation> remainingCuts;
        std::vector<AssimpModelClass::VertexType> vertices;
        std::vector<unsigned long> indices;
        bool success = false;
    };

    struct AsyncRepairResult
    {
        std::uint64_t operationId = 0;
        std::vector<AsyncRepairTargetResult> targets;
        bool success = false;
    };

    std::mutex g_asyncRepairMutex;
    std::unordered_map<std::uint64_t, AsyncRepairState> g_asyncRepairStates;

    RuntimeMeshDamageTrack* FindDamageTrack(
        ECSRegistry& registry,
        Entity entity,
        AssimpModelClass* model)
    {
        auto* damage = registry.GetComponent<RuntimeMeshDamageComponent>(entity);
        if (!damage)
            return nullptr;

        for (auto& track : damage->tracks)
        {
            if (track.model == model)
                return &track;
        }
        return nullptr;
    }

    RuntimeMeshDamageTrack* EnsureDamageTrack(
        ECSRegistry& registry,
        Entity entity,
        AssimpModelClass* model,
        ID3D11Device* device,
        const std::vector<AssimpModelClass::VertexType>& baselineVertices,
        const std::vector<unsigned long>& baselineIndices)
    {
        if (!model || baselineVertices.empty() || baselineIndices.size() < 3)
            return nullptr;

        if (!registry.HasComponent<RuntimeMeshDamageComponent>(entity))
        {
            registry.AddComponent<RuntimeMeshDamageComponent>(
                entity,
                RuntimeMeshDamageComponent{});
        }

        auto* damage = registry.GetComponent<RuntimeMeshDamageComponent>(entity);
        if (!damage)
            return nullptr;

        for (auto& track : damage->tracks)
        {
            if (track.model == model)
                return &track;
        }

        RuntimeMeshDamageTrack track;
        track.model = model;
        track.device = device;
        auto baseline = std::make_shared<RuntimeMeshGeometrySnapshot>();
        baseline->vertices = baselineVertices;
        baseline->indices = baselineIndices;
        track.baseline = std::move(baseline);
        damage->tracks.push_back(std::move(track));
        return &damage->tracks.back();
    }

    void RecordAppliedCut(
        ECSRegistry& registry,
        Entity entity,
        AssimpModelClass* model,
        const RuntimeMeshCutOperation& operation)
    {
        auto* damage = registry.GetComponent<RuntimeMeshDamageComponent>(entity);
        if (!damage)
            return;

        for (auto& track : damage->tracks)
        {
            if (track.model != model)
                continue;

            const bool alreadyRecorded = std::any_of(
                track.cuts.begin(),
                track.cuts.end(),
                [&](const RuntimeMeshCutOperation& cut)
                {
                    return cut.id == operation.id;
                });
            if (!alreadyRecorded)
            {
                track.cuts.push_back(operation);
                ++track.revision;
            }
            return;
        }
    }

    struct WeldedPosition
    {
        std::int64_t x = 0;
        std::int64_t y = 0;
        std::int64_t z = 0;

        bool operator==(const WeldedPosition& other) const
        {
            return x == other.x && y == other.y && z == other.z;
        }
    };

    struct WeldedPositionHash
    {
        size_t operator()(const WeldedPosition& value) const noexcept
        {
            const size_t hx = std::hash<std::int64_t>{}(value.x);
            const size_t hy = std::hash<std::int64_t>{}(value.y);
            const size_t hz = std::hash<std::int64_t>{}(value.z);
            return hx ^ (hy << 1) ^ (hz << 7);
        }
    };

    WeldedPosition WeldPosition(const DirectX::XMFLOAT3& position)
    {
        constexpr double inverseEpsilon = 10000.0;
        return {
            static_cast<std::int64_t>(std::llround(position.x * inverseEpsilon)),
            static_cast<std::int64_t>(std::llround(position.y * inverseEpsilon)),
            static_cast<std::int64_t>(std::llround(position.z * inverseEpsilon))
        };
    }

    float TriangleArea(
        const DirectX::XMFLOAT3& a,
        const DirectX::XMFLOAT3& b,
        const DirectX::XMFLOAT3& c)
    {
        return Length(Cross(Subtract(b, a), Subtract(c, a))) * 0.5f;
    }

    // После вырезания по краю панели иногда остаётся отдельный треугольный
    // островок. Для отдельной collision-пластины оставляем главный кусок. В
    // общем render-батче удаляем только маленькие островки рядом с новым вырезом,
    // потому что остальные несвязанные панели в этом меше являются нормальными.
    int RemoveDetachedTriangleIslands(
        std::vector<AssimpModelClass::VertexType>& vertices,
        std::vector<unsigned long>& indices,
        const DirectX::XMFLOAT3& cutCenter,
        float cutRadius,
        bool standalonePanelMesh)
    {
        const size_t triangleCount = indices.size() / 3;
        if (triangleCount < 2)
            return 0;

        std::unordered_map<WeldedPosition, std::vector<size_t>, WeldedPositionHash> trianglesByPosition;
        trianglesByPosition.reserve(triangleCount * 2);

        std::vector<std::array<WeldedPosition, 3>> trianglePositions(triangleCount);
        std::vector<bool> triangleValid(triangleCount, true);
        for (size_t triangle = 0; triangle < triangleCount; ++triangle)
        {
            std::array<WeldedPosition, 3>& positions = trianglePositions[triangle];
            for (size_t corner = 0; corner < 3; ++corner)
            {
                const unsigned long index = indices[triangle * 3 + corner];
                if (index >= vertices.size())
                {
                    triangleValid[triangle] = false;
                    break;
                }
                positions[corner] = WeldPosition(vertices[index].position);
            }

            if (!triangleValid[triangle])
                continue;

            trianglesByPosition[positions[0]].push_back(triangle);
            if (!(positions[1] == positions[0]))
                trianglesByPosition[positions[1]].push_back(triangle);
            if (!(positions[2] == positions[0]) && !(positions[2] == positions[1]))
                trianglesByPosition[positions[2]].push_back(triangle);
        }

        struct Island
        {
            std::vector<size_t> triangles;
            float area = 0.0f;
            float closestDistanceSq = (std::numeric_limits<float>::max)();
            float farthestDistanceSq = 0.0f;
        };

        std::vector<Island> islands;
        std::vector<bool> visited(triangleCount, false);
        std::vector<size_t> stack;

        for (size_t first = 0; first < triangleCount; ++first)
        {
            if (visited[first] || !triangleValid[first])
                continue;

            islands.emplace_back();
            Island& island = islands.back();
            stack.clear();
            stack.push_back(first);
            visited[first] = true;

            while (!stack.empty())
            {
                const size_t triangle = stack.back();
                stack.pop_back();
                island.triangles.push_back(triangle);

                const auto ia = indices[triangle * 3];
                const auto ib = indices[triangle * 3 + 1];
                const auto ic = indices[triangle * 3 + 2];
                const auto& a = vertices[ia].position;
                const auto& b = vertices[ib].position;
                const auto& c = vertices[ic].position;
                island.area += TriangleArea(a, b, c);

                const float distances[] = {
                    DistanceSquared(a, cutCenter),
                    DistanceSquared(b, cutCenter),
                    DistanceSquared(c, cutCenter)
                };
                for (float distanceSq : distances)
                {
                    island.closestDistanceSq = (std::min)(island.closestDistanceSq, distanceSq);
                    island.farthestDistanceSq = (std::max)(island.farthestDistanceSq, distanceSq);
                }

                for (const WeldedPosition& position : trianglePositions[triangle])
                {
                    const auto adjacent = trianglesByPosition.find(position);
                    if (adjacent == trianglesByPosition.end())
                        continue;

                    for (size_t next : adjacent->second)
                    {
                        if (!visited[next] && triangleValid[next])
                        {
                            visited[next] = true;
                            stack.push_back(next);
                        }
                    }
                }
            }
        }

        if (islands.size() <= 1)
            return 0;

        size_t largestIsland = 0;
        for (size_t i = 1; i < islands.size(); ++i)
        {
            if (islands[i].area > islands[largestIsland].area)
                largestIsland = i;
        }

        std::vector<bool> removeTriangle(triangleCount, false);
        int removedTriangles = 0;
        const float nearRadiusSq = cutRadius * cutRadius * 6.25f;
        const float fullyNearRadiusSq = cutRadius * cutRadius * 12.25f;
        const float fragmentAreaLimit = (std::max)(0.03f, cutRadius * cutRadius * 8.0f);

        for (size_t i = 0; i < islands.size(); ++i)
        {
            const Island& island = islands[i];
            const bool nearNewCut = island.closestDistanceSq <= nearRadiusSq;
            const bool fullyNearNewCut = island.farthestDistanceSq <= fullyNearRadiusSq;
            const bool remove = i != largestIsland && nearNewCut &&
                (standalonePanelMesh ||
                    (fullyNearNewCut && island.area <= fragmentAreaLimit));
            if (!remove)
                continue;

            for (size_t triangle : island.triangles)
            {
                removeTriangle[triangle] = true;
                ++removedTriangles;
            }
        }

        if (removedTriangles == 0)
            return 0;

        std::vector<unsigned long> keptIndices;
        keptIndices.reserve(indices.size() - static_cast<size_t>(removedTriangles) * 3);
        for (size_t triangle = 0; triangle < triangleCount; ++triangle)
        {
            if (removeTriangle[triangle] || !triangleValid[triangle])
                continue;
            keptIndices.push_back(indices[triangle * 3]);
            keptIndices.push_back(indices[triangle * 3 + 1]);
            keptIndices.push_back(indices[triangle * 3 + 2]);
        }

        std::vector<unsigned long> remap(vertices.size(), (std::numeric_limits<unsigned long>::max)());
        std::vector<AssimpModelClass::VertexType> compactVertices;
        compactVertices.reserve(vertices.size());
        for (unsigned long& index : keptIndices)
        {
            unsigned long& mapped = remap[index];
            if (mapped == (std::numeric_limits<unsigned long>::max)())
            {
                mapped = static_cast<unsigned long>(compactVertices.size());
                compactVertices.push_back(vertices[index]);
            }
            index = mapped;
        }

        vertices = std::move(compactVertices);
        indices = std::move(keptIndices);
        return removedTriangles;
    }

    bool BuildCircularHoleMeshData(
        const std::vector<AssimpModelClass::VertexType>& sourceVertices,
        const std::vector<unsigned long>& sourceIndices,
        const DirectX::XMFLOAT3& localCenter,
        const DirectX::XMFLOAT3& localNormal,
        float radius,
        float depth,
        bool removeDetachedFragments,
        bool standalonePanelMesh,
        std::vector<AssimpModelClass::VertexType>& outVertices,
        std::vector<unsigned long>& outIndices,
        int* changedTrianglesOut,
        int* finalTrianglesOut)
    {
        if (changedTrianglesOut)
            *changedTrianglesOut = 0;
        if (finalTrianglesOut)
            *finalTrianglesOut = 0;

        constexpr size_t maxRuntimeCutIndices = 120000;
        constexpr size_t maxExtraIndicesPerCut = 3000;

        if (sourceVertices.empty() || sourceIndices.size() < 3)
            return false;
        if (sourceIndices.size() > maxRuntimeCutIndices)
            return false;

        DirectX::XMFLOAT3 normal = NormalizeOrFallback(localNormal, { 0.0f, 1.0f, 0.0f });
        DirectX::XMFLOAT3 reference = std::fabs(normal.y) < 0.9f
            ? DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f }
            : DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 axisU = NormalizeOrFallback(Cross(reference, normal), { 1.0f, 0.0f, 0.0f });
        DirectX::XMFLOAT3 axisV = NormalizeOrFallback(Cross(normal, axisU), { 0.0f, 0.0f, 1.0f });

        constexpr int diskSegments = 20;
        const std::vector<Vec2> disk = BuildIrregularCutProfile(
            localCenter,
            radius,
            diskSegments);

        outVertices = sourceVertices;
        outIndices.clear();
        outVertices.reserve(sourceVertices.size() + 128);
        outIndices.reserve(sourceIndices.size() + 128);

        int changedTriangles = 0;

        for (size_t i = 0; i + 2 < sourceIndices.size(); i += 3)
        {
            const unsigned long ia = sourceIndices[i];
            const unsigned long ib = sourceIndices[i + 1];
            const unsigned long ic = sourceIndices[i + 2];

            if (ia >= sourceVertices.size() ||
                ib >= sourceVertices.size() ||
                ic >= sourceVertices.size())
            {
                continue;
            }

            const AssimpModelClass::VertexType& a = sourceVertices[ia];
            const AssimpModelClass::VertexType& b = sourceVertices[ib];
            const AssimpModelClass::VertexType& c = sourceVertices[ic];

            const float da = Dot(Subtract(a.position, localCenter), normal);
            const float db = Dot(Subtract(b.position, localCenter), normal);
            const float dc = Dot(Subtract(c.position, localCenter), normal);

            const bool nearCutPlane =
                std::fabs(da) <= depth ||
                std::fabs(db) <= depth ||
                std::fabs(dc) <= depth ||
                (da < 0.0f && (db > 0.0f || dc > 0.0f)) ||
                (db < 0.0f && (da > 0.0f || dc > 0.0f)) ||
                (dc < 0.0f && (da > 0.0f || db > 0.0f));

            if (!nearCutPlane)
            {
                outIndices.push_back(ia);
                outIndices.push_back(ib);
                outIndices.push_back(ic);
                continue;
            }

            auto toDisk = [&](const DirectX::XMFLOAT3& p) -> Vec2
            {
                const DirectX::XMFLOAT3 relative = Subtract(p, localCenter);
                return { Dot(relative, axisU), Dot(relative, axisV) };
            };

            if (!CircleOverlapsTriangle2D(
                toDisk(a.position),
                toDisk(b.position),
                toDisk(c.position),
                radius))
            {
                outIndices.push_back(ia);
                outIndices.push_back(ib);
                outIndices.push_back(ic);
                continue;
            }

            const bool affected = SubtractConvexDiskFromTriangle(
                a,
                b,
                c,
                localCenter,
                axisU,
                axisV,
                disk,
                outVertices,
                outIndices);

            if (!affected)
            {
                outIndices.push_back(ia);
                outIndices.push_back(ib);
                outIndices.push_back(ic);
                continue;
            }

            ++changedTriangles;
        }

        if (changedTriangles == 0 || outIndices.size() < 3)
            return false;

        if (outIndices.size() > maxRuntimeCutIndices ||
            outIndices.size() > sourceIndices.size() + maxExtraIndicesPerCut)
        {
            return false;
        }

        if (removeDetachedFragments)
        {
            const int removedFragments = RemoveDetachedTriangleIslands(
                outVertices,
                outIndices,
                localCenter,
                radius,
                standalonePanelMesh);
            changedTriangles += removedFragments;
        }

        if (outVertices.empty() || outIndices.size() < 3)
            return false;

        if (changedTrianglesOut)
            *changedTrianglesOut = changedTriangles;
        if (finalTrianglesOut)
            *finalTrianglesOut = static_cast<int>(outIndices.size() / 3);

        return true;
    }
    bool CutHitModel(
        ID3D11Device* device,
        const RayModelHit& hit,
        int* changedTriangles,
        int* finalTriangles,
        float* radiusOut,
        bool rebuildGpuBuffers)
    {
        if (!device || !hit.model)
            return false;

        const float radius = CalculateCutRadius(hit.model);
        const float depth = radius * 0.45f;

        if (radiusOut)
            *radiusOut = radius;

        return hit.model->CutCircularHoleForTest(
            device,
            hit.localPosition,
            hit.localNormal,
            radius,
            depth,
            changedTriangles,
            finalTriangles,
            rebuildGpuBuffers);
    }

    bool IsWorldPointInsideRoom(
        const DirectX::XMFLOAT3& worldPoint,
        const RoomComponent& room,
        const TransformComponent& roomTransform)
    {
        DirectX::XMMATRIX inverseRoom = DirectX::XMMatrixInverse(
            nullptr,
            roomTransform.GetWorldMatrix());

        DirectX::XMVECTOR p = DirectX::XMVector3TransformCoord(
            DirectX::XMLoadFloat3(&worldPoint),
            inverseRoom);

        DirectX::XMFLOAT3 local{};
        DirectX::XMStoreFloat3(&local, p);

        return local.x >= room.localMin.x && local.x <= room.localMax.x &&
            local.y >= room.localMin.y && local.y <= room.localMax.y &&
            local.z >= room.localMin.z && local.z <= room.localMax.z;
    }
}

bool AssimpModelClass::CutSphereHoleForTest(
    ID3D11Device* device,
    const DirectX::XMFLOAT3& localCenter,
    float radius,
    int* removedTriangles)
{
    if (removedTriangles)
        *removedTriangles = 0;

    if (!device || m_meshParts.empty() || m_cpuMeshVertices.empty() || m_cpuMeshIndices.size() < 3)
        return false;

    const float radiusSq = radius * radius;
    std::vector<unsigned long> newIndices;
    std::vector<DirectX::XMFLOAT3> newTriangles;
    newIndices.reserve(m_cpuMeshIndices.size());
    newTriangles.reserve(m_cpuTriangles.size());

    int removed = 0;

    for (size_t i = 0; i + 2 < m_cpuMeshIndices.size(); i += 3)
    {
        const unsigned long i0 = m_cpuMeshIndices[i];
        const unsigned long i1 = m_cpuMeshIndices[i + 1];
        const unsigned long i2 = m_cpuMeshIndices[i + 2];

        if (i0 >= m_cpuMeshVertices.size() ||
            i1 >= m_cpuMeshVertices.size() ||
            i2 >= m_cpuMeshVertices.size())
        {
            continue;
        }

        const DirectX::XMFLOAT3& p0 = m_cpuMeshVertices[i0].position;
        const DirectX::XMFLOAT3& p1 = m_cpuMeshVertices[i1].position;
        const DirectX::XMFLOAT3& p2 = m_cpuMeshVertices[i2].position;

        const DirectX::XMFLOAT3 centroid{
            (p0.x + p1.x + p2.x) / 3.0f,
            (p0.y + p1.y + p2.y) / 3.0f,
            (p0.z + p1.z + p2.z) / 3.0f
        };

        const bool removeTriangle =
            DistanceSquared(centroid, localCenter) <= radiusSq ||
            DistanceSquared(p0, localCenter) <= radiusSq ||
            DistanceSquared(p1, localCenter) <= radiusSq ||
            DistanceSquared(p2, localCenter) <= radiusSq;

        if (removeTriangle)
        {
            ++removed;
            continue;
        }

        newIndices.push_back(i0);
        newIndices.push_back(i1);
        newIndices.push_back(i2);
        newTriangles.push_back(p0);
        newTriangles.push_back(p1);
        newTriangles.push_back(p2);
    }

    if (removed == 0 || newIndices.size() < 3)
        return false;

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(VertexType) * static_cast<UINT>(m_cpuMeshVertices.size());
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = m_cpuMeshVertices.data();

    ID3D11Buffer* newVertexBuffer = nullptr;
    HRESULT hr = device->CreateBuffer(&vbDesc, &vbData, &newVertexBuffer);
    if (FAILED(hr))
        return false;

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(unsigned long) * static_cast<UINT>(newIndices.size());
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = newIndices.data();

    ID3D11Buffer* newIndexBuffer = nullptr;
    hr = device->CreateBuffer(&ibDesc, &ibData, &newIndexBuffer);
    if (FAILED(hr))
    {
        newVertexBuffer->Release();
        return false;
    }

    MeshPart& part = m_meshParts[0];
    if (part.vertexBuffer)
        part.vertexBuffer->Release();
    if (part.indexBuffer)
        part.indexBuffer->Release();

    part.vertexBuffer = newVertexBuffer;
    part.indexBuffer = newIndexBuffer;
    part.indexCount = static_cast<int>(newIndices.size());

    m_cpuMeshIndices = std::move(newIndices);
    m_cpuTriangles = std::move(newTriangles);
    m_totalIndexCount = part.indexCount;
    m_boundsCached = false;

    if (removedTriangles)
        *removedTriangles = removed;

    return true;
}

bool AssimpModelClass::CutCircularHoleForTest(
    ID3D11Device* device,
    const DirectX::XMFLOAT3& localCenter,
    const DirectX::XMFLOAT3& localNormal,
    float radius,
    float depth,
    int* removedTriangles,
    int* createdTriangles,
    bool rebuildGpuBuffers)
{
    if (removedTriangles)
        *removedTriangles = 0;
    if (createdTriangles)
        *createdTriangles = 0;

    if (!device || m_meshParts.empty() || m_cpuMeshVertices.empty() || m_cpuMeshIndices.size() < 3)
        return false;

    constexpr size_t maxRuntimeCutIndices = 120000;
    constexpr size_t maxExtraIndicesPerCut = 3000;
    if (m_cpuMeshIndices.size() > maxRuntimeCutIndices)
    {
        OutputDebugStringA("[MeshCuttingTest] Cut skipped: mesh is already too dense.\n");
        return false;
    }

    DirectX::XMFLOAT3 normal = NormalizeOrFallback(localNormal, { 0.0f, 1.0f, 0.0f });
    DirectX::XMFLOAT3 reference = std::fabs(normal.y) < 0.9f
        ? DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f }
        : DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 axisU = NormalizeOrFallback(Cross(reference, normal), { 1.0f, 0.0f, 0.0f });
    DirectX::XMFLOAT3 axisV = NormalizeOrFallback(Cross(normal, axisU), { 0.0f, 0.0f, 1.0f });

    constexpr int diskSegments = 20;
    const std::vector<Vec2> disk = BuildIrregularCutProfile(
        localCenter,
        radius,
        diskSegments);

    std::vector<VertexType> newVertices = m_cpuMeshVertices;
    std::vector<unsigned long> newIndices;
    newVertices.reserve(m_cpuMeshVertices.size() + 128);
    newIndices.reserve(m_cpuMeshIndices.size() + 128);

    int changedTriangles = 0;

    for (size_t i = 0; i + 2 < m_cpuMeshIndices.size(); i += 3)
    {
        const unsigned long ia = m_cpuMeshIndices[i];
        const unsigned long ib = m_cpuMeshIndices[i + 1];
        const unsigned long ic = m_cpuMeshIndices[i + 2];

        if (ia >= m_cpuMeshVertices.size() ||
            ib >= m_cpuMeshVertices.size() ||
            ic >= m_cpuMeshVertices.size())
        {
            continue;
        }

        const VertexType& a = m_cpuMeshVertices[ia];
        const VertexType& b = m_cpuMeshVertices[ib];
        const VertexType& c = m_cpuMeshVertices[ic];

        const float da = Dot(Subtract(a.position, localCenter), normal);
        const float db = Dot(Subtract(b.position, localCenter), normal);
        const float dc = Dot(Subtract(c.position, localCenter), normal);

        const bool nearCutPlane =
            std::fabs(da) <= depth ||
            std::fabs(db) <= depth ||
            std::fabs(dc) <= depth ||
            (da < 0.0f && (db > 0.0f || dc > 0.0f)) ||
            (db < 0.0f && (da > 0.0f || dc > 0.0f)) ||
            (dc < 0.0f && (da > 0.0f || db > 0.0f));

        if (!nearCutPlane)
        {
            newIndices.push_back(ia);
            newIndices.push_back(ib);
            newIndices.push_back(ic);
            continue;
        }

        auto toDisk = [&](const DirectX::XMFLOAT3& p) -> Vec2
        {
            const DirectX::XMFLOAT3 relative = Subtract(p, localCenter);
            return { Dot(relative, axisU), Dot(relative, axisV) };
        };

        if (!CircleOverlapsTriangle2D(
            toDisk(a.position),
            toDisk(b.position),
            toDisk(c.position),
            radius))
        {
            newIndices.push_back(ia);
            newIndices.push_back(ib);
            newIndices.push_back(ic);
            continue;
        }

        const bool affected = SubtractConvexDiskFromTriangle(
            a,
            b,
            c,
            localCenter,
            axisU,
            axisV,
            disk,
            newVertices,
            newIndices);

        if (!affected)
        {
            newIndices.push_back(ia);
            newIndices.push_back(ib);
            newIndices.push_back(ic);
            continue;
        }

        ++changedTriangles;
    }

    if (changedTriangles == 0 || newIndices.size() < 3)
        return false;

    if (newIndices.size() > maxRuntimeCutIndices ||
        newIndices.size() > m_cpuMeshIndices.size() + maxExtraIndicesPerCut)
    {
        OutputDebugStringA("[MeshCuttingTest] Cut skipped: mesh would grow too much.\n");
        return false;
    }

    if (!rebuildGpuBuffers)
    {
        m_cpuMeshVertices = std::move(newVertices);
        m_cpuMeshIndices = std::move(newIndices);
        RebuildCpuPositionCaches(m_cpuMeshVertices, m_cpuMeshIndices, m_cpuVertices, m_cpuTriangles);

        m_totalIndexCount = static_cast<int>(m_cpuMeshIndices.size());

        if (removedTriangles)
            *removedTriangles = changedTriangles;
        if (createdTriangles)
            *createdTriangles = static_cast<int>(m_cpuMeshIndices.size() / 3);

        return true;
    }

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(VertexType) * static_cast<UINT>(newVertices.size());
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = newVertices.data();

    ID3D11Buffer* newVertexBuffer = nullptr;
    HRESULT hr = device->CreateBuffer(&vbDesc, &vbData, &newVertexBuffer);
    if (FAILED(hr))
        return false;

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(unsigned long) * static_cast<UINT>(newIndices.size());
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = newIndices.data();

    ID3D11Buffer* newIndexBuffer = nullptr;
    hr = device->CreateBuffer(&ibDesc, &ibData, &newIndexBuffer);
    if (FAILED(hr))
    {
        newVertexBuffer->Release();
        return false;
    }

    MeshPart& part = m_meshParts[0];
    if (part.vertexBuffer)
        part.vertexBuffer->Release();
    if (part.indexBuffer)
        part.indexBuffer->Release();

    part.vertexBuffer = newVertexBuffer;
    part.indexBuffer = newIndexBuffer;
    part.indexCount = static_cast<int>(newIndices.size());

    m_cpuMeshVertices = std::move(newVertices);
    m_cpuMeshIndices = std::move(newIndices);
    RebuildCpuPositionCaches(m_cpuMeshVertices, m_cpuMeshIndices, m_cpuVertices, m_cpuTriangles);

    m_totalIndexCount = part.indexCount;

    if (removedTriangles)
        *removedTriangles = changedTriangles;
    if (createdTriangles)
        *createdTriangles = part.indexCount / 3;

    return true;
}

bool AssimpModelClass::ApplyCpuMeshForTest(
    ID3D11Device* device,
    std::vector<VertexType>&& vertices,
    std::vector<unsigned long>&& indices)
{
    if (!device || m_meshParts.empty() || vertices.empty() || indices.size() < 3)
        return false;

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(VertexType) * static_cast<UINT>(vertices.size());
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices.data();

    ID3D11Buffer* newVertexBuffer = nullptr;
    HRESULT hr = device->CreateBuffer(&vbDesc, &vbData, &newVertexBuffer);
    if (FAILED(hr))
        return false;

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(unsigned long) * static_cast<UINT>(indices.size());
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices.data();

    ID3D11Buffer* newIndexBuffer = nullptr;
    hr = device->CreateBuffer(&ibDesc, &ibData, &newIndexBuffer);
    if (FAILED(hr))
    {
        newVertexBuffer->Release();
        return false;
    }

    MeshPart& part = m_meshParts[0];
    if (part.vertexBuffer)
        part.vertexBuffer->Release();
    if (part.indexBuffer)
        part.indexBuffer->Release();

    part.vertexBuffer = newVertexBuffer;
    part.indexBuffer = newIndexBuffer;
    part.indexCount = static_cast<int>(indices.size());

    m_cpuMeshVertices = std::move(vertices);
    m_cpuMeshIndices = std::move(indices);
    RebuildCpuPositionCaches(m_cpuMeshVertices, m_cpuMeshIndices, m_cpuVertices, m_cpuTriangles);

    m_totalIndexCount = part.indexCount;
    m_boundsCached = false;

    return true;
}
bool MeshCuttingTest::CutRandomHoleInReactor(ECSRegistry& registry, ID3D11Device* device)
{
    if (!device)
        return false;

    struct Candidate
    {
        Entity entity = INVALID_ENTITY;
        MeshRendererComponent* mesh = nullptr;
        int priority = 0;
    };

    std::vector<Candidate> candidates;
    RoomComponent* reactorRoom = nullptr;
    TransformComponent* reactorTransform = nullptr;

    auto& rooms = registry.GetComponents<RoomComponent>();
    for (auto& [entity, room] : rooms)
    {
        if (!ContainsNoCase(room.roomName, "REACTOR"))
            continue;

        auto* transform = registry.GetComponent<TransformComponent>(entity);
        if (!transform)
            continue;

        reactorRoom = &room;
        reactorTransform = transform;
        break;
    }

    auto& names = registry.GetComponents<NameComponent>();
    for (auto& [entity, name] : names)
    {
        if (!ContainsNoCase(name.name, "REACTOR"))
            continue;

        auto* mesh = registry.GetComponent<MeshRendererComponent>(entity);
        if (!mesh || !mesh->model)
            continue;

        candidates.push_back({ entity, mesh, 3 });
    }

    if (reactorRoom && reactorTransform)
    {
        auto& meshes = registry.GetComponents<MeshRendererComponent>();
        for (auto& [entity, mesh] : meshes)
        {
            if (!mesh.model)
                continue;

            auto* transform = registry.GetComponent<TransformComponent>(entity);
            if (!transform)
                continue;

            if (!IsWorldPointInsideRoom(transform->position, *reactorRoom, *reactorTransform))
                continue;

            candidates.push_back({ entity, &mesh, 2 });
        }
    }

    auto& meshes = registry.GetComponents<MeshRendererComponent>();
    for (auto& [entity, mesh] : meshes)
    {
        if (!mesh.model)
            continue;

        candidates.push_back({ entity, &mesh, 1 });
    }

    if (candidates.empty())
    {
        OutputDebugStringA("[MeshCuttingTest] No mesh candidates found.\n");
        return false;
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const Candidate& a, const Candidate& b)
        {
            return a.priority > b.priority;
        });

    static std::mt19937 rng{ std::random_device{}() };

    int attempts = 0;
    for (const Candidate& candidate : candidates)
    {
        if (!candidate.mesh || !candidate.mesh->model)
            continue;

        ++attempts;

        std::vector<DirectX::XMFLOAT3> triangles;
        candidate.mesh->model->GetRawTriangles(triangles);
        if (triangles.size() < 3)
            continue;

        const size_t triangleCount = triangles.size() / 3;
        std::uniform_int_distribution<size_t> triDist(0, triangleCount - 1);

        const size_t triIndex = triDist(rng) * 3;
        DirectX::XMFLOAT3 center{
            (triangles[triIndex].x + triangles[triIndex + 1].x + triangles[triIndex + 2].x) / 3.0f,
            (triangles[triIndex].y + triangles[triIndex + 1].y + triangles[triIndex + 2].y) / 3.0f,
            (triangles[triIndex].z + triangles[triIndex + 1].z + triangles[triIndex + 2].z) / 3.0f
        };

        DirectX::XMFLOAT3 bbMin{};
        DirectX::XMFLOAT3 bbMax{};
        candidate.mesh->model->GetBoundingBox(&bbMin, &bbMax);
        const DirectX::XMFLOAT3 size{
            bbMax.x - bbMin.x,
            bbMax.y - bbMin.y,
            bbMax.z - bbMin.z
        };

        float radius = Length(size) * 0.18f;
        if (radius < 0.5f)
            radius = 0.5f;

        int removedTriangles = 0;
        if (!candidate.mesh->model->CutSphereHoleForTest(device, center, radius, &removedTriangles))
            continue;

        if (auto* rb = registry.GetComponent<RigidBodyComponent>(candidate.entity))
            PhysicsSystem::RefreshRigidBody(registry, candidate.entity, rb);

        char buffer[384];
        sprintf_s(buffer,
            "[MeshCuttingTest] Cut entity=%u attempts=%d priority=%d center=(%.2f %.2f %.2f) radius=%.2f removed=%d\n",
            candidate.entity,
            attempts,
            candidate.priority,
            center.x,
            center.y,
            center.z,
            radius,
            removedTriangles);
        OutputDebugStringA(buffer);

        return true;
    }

    char buffer[256];
    sprintf_s(buffer,
        "[MeshCuttingTest] Cut failed. candidates=%zu attempts=%d\n",
        candidates.size(),
        attempts);
    OutputDebugStringA(buffer);
    return false;
}

bool MeshCuttingTest::CutByRayAsync(
    ECSRegistry& registry,
    ID3D11Device* device,
    JobSystem& jobs,
    const CameraClass::Ray& ray,
    float radiusOverride,
    bool cutThrough,
    std::uint64_t* operationIdOut)
{
    if (!device)
        return false;

    const auto requestStart = std::chrono::steady_clock::now();

    RayModelHit closestVisual;
    RayModelHit closestPhysics;
    std::vector<RayModelHit> panelVisualHits;
    auto& meshes = registry.GetComponents<MeshRendererComponent>();

    for (auto& [entity, mesh] : meshes)
    {
        if (!mesh.model)
            continue;
        if (!registry.HasComponent<ActiveComponent>(entity))
            continue;

        auto* transform = registry.GetComponent<TransformComponent>(entity);
        if (!transform)
            continue;

        RaycastModel(entity, mesh.model, *transform, ray, closestVisual);

        if (registry.HasComponent<ShipPanelVisualBatchComponent>(entity))
        {
            RayModelHit materialBatchHit;
            RaycastModel(entity, mesh.model, *transform, ray, materialBatchHit);
            if (materialBatchHit.entity != INVALID_ENTITY)
                panelVisualHits.push_back(materialBatchHit);
        }
    }

    auto& rigidBodies = registry.GetComponents<RigidBodyComponent>();
    for (auto& [entity, rb] : rigidBodies)
    {
        if (registry.HasComponent<ActiveComponent>(entity) == false)
            continue;

        auto* transform = registry.GetComponent<TransformComponent>(entity);
        if (!transform)
            continue;

        AssimpModelClass* physicsModel = nullptr;
        if (rb.useSeparateCollisionMesh && rb.collisionModel)
            physicsModel = rb.collisionModel;
        else if (auto* mesh = registry.GetComponent<MeshRendererComponent>(entity))
            physicsModel = mesh->model;

        RaycastModel(entity, physicsModel, *transform, ray, closestPhysics);
    }

    if (closestVisual.entity == INVALID_ENTITY && closestPhysics.entity == INVALID_ENTITY)
    {
        OutputDebugStringA("[MeshCuttingTest] Async ray cut missed render and collision meshes.\n");
        return false;
    }

    const Entity operationOwnerPanel =
        closestPhysics.entity != INVALID_ENTITY &&
        registry.HasComponent<HullPanelComponent>(closestPhysics.entity)
            ? closestPhysics.entity
            : INVALID_ENTITY;
    const std::uint64_t operationId = g_nextCutOperationId.fetch_add(1);

    auto submitCut = [&](const RayModelHit& hit, float depthOverride = 0.0f) -> bool
    {
        if (hit.entity == INVALID_ENTITY || !hit.model)
            return false;

        if (!TryMarkCutPending(hit.model))
        {
            OutputDebugStringA("[MeshCuttingTest] Async cut skipped: model already has a pending cut.\n");
            return false;
        }

        // F6 passes zero and keeps the model-derived test radius. Ship weapons
        // pass their caliber-sized radius, so gameplay can create larger breaches.
        const float radius = radiusOverride > 0.0f
            ? radiusOverride
            : CalculateCutRadius(hit.model);
        // A ship projectile cuts a cylinder along its flight direction through the
        // complete model bounds, removing both skins of a box-shaped hull panel.
        // F6 keeps the original shallow cut along the surface normal.
        const DirectX::XMFLOAT3 cutDirection = cutThrough
            ? hit.localRayDirection
            : hit.localNormal;
        const float depth = cutThrough
            ? (depthOverride > 0.0f
                ? depthOverride
                : CalculateThroughCutDepth(hit.model, hit.localPosition, cutDirection, radius))
            : radius * 0.45f;
        auto sourceVertices = hit.model->GetCpuMeshVerticesForTest();
        auto sourceIndices = hit.model->GetCpuMeshIndicesForTest();
        RuntimeMeshDamageTrack* damageTrack = EnsureDamageTrack(
            registry,
            hit.entity,
            hit.model,
            device,
            sourceVertices,
            sourceIndices);
        if (!damageTrack)
        {
            ClearCutPending(hit.model);
            return false;
        }

        AssimpModelClass* targetModel = hit.model;
        Entity targetEntity = hit.entity;
        DirectX::XMFLOAT3 localPosition = hit.localPosition;
        DirectX::XMFLOAT3 localNormal = cutDirection;
        RuntimeMeshCutOperation operation;
        operation.id = operationId;
        operation.ownerPanel = operationOwnerPanel != INVALID_ENTITY
            ? operationOwnerPanel
            : hit.entity;
        operation.localCenter = localPosition;
        operation.localNormal = localNormal;
        operation.radius = radius;
        operation.depth = depth;
        operation.standalonePanelMesh =
            registry.HasComponent<HullPanelComponent>(hit.entity);
        const bool cleanDetachedFragments = operationOwnerPanel != INVALID_ENTITY;

        const bool submitted = jobs.SubmitJob([
            &registry,
            &jobs,
            device,
            targetModel,
            targetEntity,
            localPosition,
            localNormal,
            radius,
            depth,
            operation,
            cleanDetachedFragments,
            sourceVertices = std::move(sourceVertices),
            sourceIndices = std::move(sourceIndices),
            requestStart]() mutable
        {
            auto result = std::make_shared<AsyncCutResult>();
            result->entity = targetEntity;
            result->model = targetModel;
            result->radius = radius;
            result->operation = operation;
            result->success = BuildCircularHoleMeshData(
                sourceVertices,
                sourceIndices,
                localPosition,
                localNormal,
                radius,
                depth,
                cleanDetachedFragments,
                operation.standalonePanelMesh,
                result->vertices,
                result->indices,
                &result->changedTriangles,
                &result->finalTriangles);

            jobs.SubmitMainThread([
                &registry,
                device,
                result,
                requestStart]() mutable
            {
                const auto endTime = std::chrono::steady_clock::now();
                const double totalMs = std::chrono::duration<double, std::milli>(endTime - requestStart).count();

                if (!result->success)
                {
                    ClearCutPending(result->model);
                    OutputDebugStringA("[MeshCuttingTest] Async cut worker finished, but clipping did not change mesh.\n");
                    return;
                }

                bool modelStillUsedByEntity = false;
                if (auto* mesh = registry.GetComponent<MeshRendererComponent>(result->entity))
                    modelStillUsedByEntity = modelStillUsedByEntity || mesh->model == result->model;
                if (auto* rb = registry.GetComponent<RigidBodyComponent>(result->entity))
                    modelStillUsedByEntity = modelStillUsedByEntity || rb->collisionModel == result->model;

                if (!registry.HasComponent<ActiveComponent>(result->entity) || !modelStillUsedByEntity)
                {
                    ClearCutPending(result->model);
                    OutputDebugStringA("[MeshCuttingTest] Async cut result ignored: entity/model changed before apply.\n");
                    return;
                }

                const bool applied = result->model->ApplyCpuMeshForTest(
                    device,
                    std::move(result->vertices),
                    std::move(result->indices));

                if (applied)
                {
                    RecordAppliedCut(
                        registry,
                        result->entity,
                        result->model,
                        result->operation);

                    if (auto* rb = registry.GetComponent<RigidBodyComponent>(result->entity))
                        PhysicsSystem::RefreshRigidBody(registry, result->entity, rb);

                    char buffer[384];
                    sprintf_s(buffer,
                        "[MeshCuttingTest] Async ray cut %.2f ms entity=%u changed=%d final=%d radius=%.2f\n",
                        totalMs,
                        result->entity,
                        result->changedTriangles,
                        result->finalTriangles,
                        result->radius);
                    OutputDebugStringA(buffer);
                }
                else
                {
                    OutputDebugStringA("[MeshCuttingTest] Async cut apply failed on main thread.\n");
                }

                ClearCutPending(result->model);
            });
        });

        if (!submitted)
            ClearCutPending(targetModel);

        return submitted;
    };

    // The visual hull is a single ship-wide batch, while the physics hit belongs
    // to one panel. Use the panel thickness for the visual cut, otherwise the
    // batch bounds let the cylinder reach a wall on the far side of the ship.
    float panelWorldDepth = 0.0f;
    if (cutThrough &&
        closestVisual.entity != INVALID_ENTITY &&
        closestPhysics.entity != INVALID_ENTITY &&
        closestVisual.model != closestPhysics.model)
    {
        const auto* visualTransform = registry.GetComponent<TransformComponent>(closestVisual.entity);
        const auto* physicsTransform = registry.GetComponent<TransformComponent>(closestPhysics.entity);
        if (visualTransform && physicsTransform)
        {
            const float physicsLocalDepth = CalculateThroughCutDepth(
                closestPhysics.model,
                closestPhysics.localPosition,
                closestPhysics.localRayDirection,
                radiusOverride > 0.0f ? radiusOverride : CalculateCutRadius(closestPhysics.model));
            panelWorldDepth = ConvertLocalDepthToWorld(
                *physicsTransform,
                closestPhysics.localRayDirection,
                physicsLocalDepth);
        }
    }

    bool submittedAny = false;
    const bool physicsHitPanel = closestPhysics.entity != INVALID_ENTITY &&
        registry.HasComponent<HullPanelComponent>(closestPhysics.entity);

    if (cutThrough && physicsHitPanel && panelWorldDepth > 0.0f && !panelVisualHits.empty())
    {
        const float effectiveRadius = radiusOverride > 0.0f
            ? radiusOverride
            : CalculateCutRadius(closestPhysics.model);
        // The visual and collision layers are imported independently. Give the
        // range a full caliber margin so a second hit, whose first skin is
        // already gone, still reaches the remaining skin of the same panel.
        const float distancePadding = (std::max)(0.04f, effectiveRadius);
        const float minDistance = closestPhysics.worldDistance - distancePadding;
        const float maxDistance = closestPhysics.worldDistance + panelWorldDepth + distancePadding;

        for (const RayModelHit& materialHit : panelVisualHits)
        {
            if (materialHit.worldDistance < minDistance || materialHit.worldDistance > maxDistance)
                continue;

            const auto* visualTransform = registry.GetComponent<TransformComponent>(materialHit.entity);
            if (!visualTransform)
                continue;

            const float materialDepth = ConvertWorldDepthToLocal(
                *visualTransform,
                ray.direction,
                panelWorldDepth + distancePadding);
            submittedAny = submitCut(materialHit, materialDepth) || submittedAny;
        }
    }
    else
    {
        float visualDepthOverride = 0.0f;
        if (cutThrough && panelWorldDepth > 0.0f && closestVisual.entity != INVALID_ENTITY)
        {
            if (const auto* visualTransform = registry.GetComponent<TransformComponent>(closestVisual.entity))
            {
                visualDepthOverride = ConvertWorldDepthToLocal(
                    *visualTransform,
                    ray.direction,
                    panelWorldDepth);
            }
        }
        submittedAny = submitCut(closestVisual, visualDepthOverride) || submittedAny;
    }

    if (closestPhysics.entity != INVALID_ENTITY && closestPhysics.model != closestVisual.model)
        submittedAny = submitCut(closestPhysics) || submittedAny;

    if (submittedAny && operationIdOut)
        *operationIdOut = operationId;

    return submittedAny;
}

bool MeshCuttingTest::RepairCut(ECSRegistry& registry, std::uint64_t operationId)
{
    if (operationId == 0)
        return true;

    struct RebuildTarget
    {
        Entity entity = INVALID_ENTITY;
        RuntimeMeshDamageTrack* track = nullptr;
        std::vector<AssimpModelClass::VertexType> vertices;
        std::vector<unsigned long> indices;
        std::vector<RuntimeMeshCutOperation> remainingCuts;
    };

    std::vector<RebuildTarget> targets;

    for (auto& [entity, damage] : registry.GetComponents<RuntimeMeshDamageComponent>())
    {
        for (auto& track : damage.tracks)
        {
            // Операция появляется в cuts только после успешного применения на
            // главном потоке. До этого момента не разрешаем ремонту обогнать job.
            if (track.model && IsCutPending(track.model))
            {
                OutputDebugStringA("[MeshRepair] Repair postponed until pending mesh cut finishes.\n");
                return false;
            }

            const auto repairedCut = std::find_if(
                track.cuts.begin(),
                track.cuts.end(),
                [&](const RuntimeMeshCutOperation& cut)
                {
                    return cut.id == operationId;
                });
            if (repairedCut == track.cuts.end())
                continue;

            if (!track.model || !track.device ||
                !track.baseline || track.baseline->vertices.empty() ||
                track.baseline->indices.size() < 3 ||
                IsCutPending(track.model))
            {
                OutputDebugStringA("[MeshRepair] Repair postponed: mesh is invalid or still being cut.\n");
                return false;
            }

            RebuildTarget target;
            target.entity = entity;
            target.track = &track;
            target.vertices = track.baseline->vertices;
            target.indices = track.baseline->indices;

            for (const RuntimeMeshCutOperation& cut : track.cuts)
            {
                if (cut.id == operationId)
                    continue;

                std::vector<AssimpModelClass::VertexType> nextVertices;
                std::vector<unsigned long> nextIndices;
                int changedTriangles = 0;
                int finalTriangles = 0;
                const bool rebuilt = BuildCircularHoleMeshData(
                    target.vertices,
                    target.indices,
                    cut.localCenter,
                    cut.localNormal,
                    cut.radius,
                    cut.depth,
                    true,
                    cut.standalonePanelMesh,
                    nextVertices,
                    nextIndices,
                    &changedTriangles,
                    &finalTriangles);
                if (!rebuilt)
                {
                    OutputDebugStringA("[MeshRepair] Remaining cut could not be replayed; repair cancelled.\n");
                    return false;
                }

                target.vertices = std::move(nextVertices);
                target.indices = std::move(nextIndices);
                target.remainingCuts.push_back(cut);
            }

            targets.push_back(std::move(target));
        }
    }

    if (targets.empty())
    {
        // Старые сохранения и очень редкий неудачный async-cut не имеют истории.
        // Игровой ремонт всё равно разрешаем, но явно оставляем сообщение в логе.
        OutputDebugStringA("[MeshRepair] Cut history not found; gameplay breach will still be sealed.\n");
        return true;
    }

    for (RebuildTarget& target : targets)
    {
        if (!target.track->model->ApplyCpuMeshForTest(
            target.track->device,
            std::move(target.vertices),
            std::move(target.indices)))
        {
            OutputDebugStringA("[MeshRepair] DirectX mesh rebuild failed.\n");
            return false;
        }

        target.track->cuts = std::move(target.remainingCuts);
        ++target.track->revision;

        if (auto* rb = registry.GetComponent<RigidBodyComponent>(target.entity))
            PhysicsSystem::RefreshRigidBody(registry, target.entity, rb);
    }

    OutputDebugStringA("[MeshRepair] Runtime mesh and collision restored.\n");
    return true;
}

bool MeshCuttingTest::RepairCutAsync(
    ECSRegistry& registry,
    JobSystem& jobs,
    std::uint64_t operationId)
{
    if (operationId == 0)
        return true;

    // The caller polls this function while repair progress is at 100%.
    // Completed is consumed once; Pending returns immediately without creating
    // duplicate jobs; Failed is removed so a later frame can retry safely.
    {
        std::lock_guard<std::mutex> lock(g_asyncRepairMutex);
        const auto stateIt = g_asyncRepairStates.find(operationId);
        if (stateIt != g_asyncRepairStates.end())
        {
            if (stateIt->second == AsyncRepairState::Pending)
                return false;
            if (stateIt->second == AsyncRepairState::Completed)
            {
                g_asyncRepairStates.erase(stateIt);
                return true;
            }
            g_asyncRepairStates.erase(stateIt);
        }
    }

    std::vector<AsyncRepairInput> inputs;
    std::unordered_set<AssimpModelClass*> uniqueModels;

    // Main thread: capture only immutable snapshots and small cut histories.
    // The expensive geometry copies/replay happen after SubmitJob.
    for (auto& [entity, damage] : registry.GetComponents<RuntimeMeshDamageComponent>())
    {
        for (auto& track : damage.tracks)
        {
            const auto repairedCut = std::find_if(
                track.cuts.begin(),
                track.cuts.end(),
                [&](const RuntimeMeshCutOperation& cut)
                {
                    return cut.id == operationId;
                });
            if (repairedCut == track.cuts.end())
                continue;

            // The same pending set serializes cutting and repair for a model.
            // A repair must never read geometry while a cut worker is replacing it.
            if (!track.model || !track.device || !track.baseline ||
                track.baseline->vertices.empty() || track.baseline->indices.size() < 3 ||
                IsCutPending(track.model))
            {
                OutputDebugStringA("[MeshRepair] Async repair postponed: model is busy or invalid.\n");
                return false;
            }

            AsyncRepairInput input;
            input.entity = entity;
            input.model = track.model;
            input.device = track.device;
            input.expectedRevision = track.revision;
            input.baseline = track.baseline;
            input.remainingCuts.reserve(track.cuts.size());
            for (const RuntimeMeshCutOperation& cut : track.cuts)
            {
                if (cut.id != operationId)
                    input.remainingCuts.push_back(cut);
            }
            inputs.push_back(std::move(input));
            uniqueModels.insert(track.model);
        }
    }

    if (inputs.empty())
    {
        OutputDebugStringA("[MeshRepair] Cut history not found; gameplay breach will still be sealed.\n");
        return true;
    }

    std::vector<AssimpModelClass*> markedModels;
    markedModels.reserve(uniqueModels.size());
    for (AssimpModelClass* model : uniqueModels)
    {
        if (!TryMarkCutPending(model))
        {
            for (AssimpModelClass* marked : markedModels)
                ClearCutPending(marked);
            return false;
        }
        markedModels.push_back(model);
    }

    {
        std::lock_guard<std::mutex> lock(g_asyncRepairMutex);
        g_asyncRepairStates[operationId] = AsyncRepairState::Pending;
    }

    const auto requestStart = std::chrono::steady_clock::now();
    const bool submitted = jobs.SubmitJob([
        &registry,
        &jobs,
        operationId,
        inputs = std::move(inputs),
        requestStart]() mutable
    {
        auto result = std::make_shared<AsyncRepairResult>();
        result->operationId = operationId;
        result->success = true;
        result->targets.reserve(inputs.size());

        for (AsyncRepairInput& input : inputs)
        {
            AsyncRepairTargetResult target;
            target.entity = input.entity;
            target.model = input.model;
            target.device = input.device;
            target.expectedRevision = input.expectedRevision;
            target.remainingCuts = std::move(input.remainingCuts);

            // This first full geometry copy used to happen on the render thread
            // exactly when the progress bar reached 100%.
            target.vertices = input.baseline->vertices;
            target.indices = input.baseline->indices;
            target.success = true;

            for (const RuntimeMeshCutOperation& cut : target.remainingCuts)
            {
                std::vector<AssimpModelClass::VertexType> nextVertices;
                std::vector<unsigned long> nextIndices;
                int changedTriangles = 0;
                int finalTriangles = 0;
                if (!BuildCircularHoleMeshData(
                    target.vertices,
                    target.indices,
                    cut.localCenter,
                    cut.localNormal,
                    cut.radius,
                    cut.depth,
                    true,
                    cut.standalonePanelMesh,
                    nextVertices,
                    nextIndices,
                    &changedTriangles,
                    &finalTriangles))
                {
                    target.success = false;
                    result->success = false;
                    break;
                }

                target.vertices = std::move(nextVertices);
                target.indices = std::move(nextIndices);
            }

            result->targets.push_back(std::move(target));
        }

        jobs.SubmitMainThread([
            &registry,
            result,
            requestStart]() mutable
        {
            bool appliedAll = result->success;

            // Validate every target before changing the first model. Another
            // operation may have changed a revision while the worker was busy.
            if (appliedAll)
            {
                for (const AsyncRepairTargetResult& target : result->targets)
                {
                    RuntimeMeshDamageTrack* track = FindDamageTrack(
                        registry, target.entity, target.model);
                    const bool operationStillExists = track && std::any_of(
                        track->cuts.begin(),
                        track->cuts.end(),
                        [&](const RuntimeMeshCutOperation& cut)
                        {
                            return cut.id == result->operationId;
                        });
                    if (!track || track->revision != target.expectedRevision ||
                        !operationStillExists || !target.success)
                    {
                        appliedAll = false;
                        break;
                    }
                }
            }

            if (appliedAll)
            {
                for (AsyncRepairTargetResult& target : result->targets)
                {
                    RuntimeMeshDamageTrack* track = FindDamageTrack(
                        registry, target.entity, target.model);
                    if (!track || !target.model->ApplyCpuMeshForTest(
                        target.device,
                        std::move(target.vertices),
                        std::move(target.indices)))
                    {
                        appliedAll = false;
                        break;
                    }

                    track->cuts = std::move(target.remainingCuts);
                    ++track->revision;

                    // Bullet world membership must stay on the main thread. The
                    // expensive cut replay is already complete at this point.
                    if (auto* rb = registry.GetComponent<RigidBodyComponent>(target.entity))
                        PhysicsSystem::RefreshRigidBody(registry, target.entity, rb);
                }
            }

            for (const AsyncRepairTargetResult& target : result->targets)
                ClearCutPending(target.model);

            {
                std::lock_guard<std::mutex> lock(g_asyncRepairMutex);
                g_asyncRepairStates[result->operationId] = appliedAll
                    ? AsyncRepairState::Completed
                    : AsyncRepairState::Failed;
            }

            const double totalMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - requestStart).count();
            char buffer[320];
            sprintf_s(buffer,
                "[MeshRepair] Async rebuild %s in %.2f ms, operation=%llu targets=%zu\n",
                appliedAll ? "completed" : "failed",
                totalMs,
                static_cast<unsigned long long>(result->operationId),
                result->targets.size());
            OutputDebugStringA(buffer);
        });
    });

    if (!submitted)
    {
        for (AssimpModelClass* model : markedModels)
            ClearCutPending(model);
        std::lock_guard<std::mutex> lock(g_asyncRepairMutex);
        g_asyncRepairStates.erase(operationId);
    }

    return false;
}

bool MeshCuttingTest::CutByRay(ECSRegistry& registry, ID3D11Device* device, const CameraClass::Ray& ray)
{
    if (!device)
        return false;

    const auto cutStart = std::chrono::steady_clock::now();

    RayModelHit closestVisual;
    RayModelHit closestPhysics;
    auto& meshes = registry.GetComponents<MeshRendererComponent>();

    for (auto& [entity, mesh] : meshes)
    {
        if (!mesh.model)
            continue;
        if (!registry.HasComponent<ActiveComponent>(entity))
            continue;

        auto* transform = registry.GetComponent<TransformComponent>(entity);
        if (!transform)
            continue;

        RaycastModel(entity, mesh.model, *transform, ray, closestVisual);
    }

    auto& rigidBodies = registry.GetComponents<RigidBodyComponent>();
    for (auto& [entity, rb] : rigidBodies)
    {
        if (registry.HasComponent<ActiveComponent>(entity) == false)
            continue;

        auto* transform = registry.GetComponent<TransformComponent>(entity);
        if (!transform)
            continue;

        AssimpModelClass* physicsModel = nullptr;
        if (rb.useSeparateCollisionMesh && rb.collisionModel)
            physicsModel = rb.collisionModel;
        else if (auto* mesh = registry.GetComponent<MeshRendererComponent>(entity))
            physicsModel = mesh->model;

        RaycastModel(entity, physicsModel, *transform, ray, closestPhysics);
    }

    if (closestVisual.entity == INVALID_ENTITY && closestPhysics.entity == INVALID_ENTITY)
    {
        OutputDebugStringA("[MeshCuttingTest] Ray cut missed render and collision meshes.\n");
        return false;
    }

    bool changedAny = false;
    AssimpModelClass* visualCutModel = nullptr;

    int visualChangedTriangles = 0;
    int visualFinalTriangles = 0;
    float visualRadius = 0.0f;
    if (closestVisual.entity != INVALID_ENTITY && closestVisual.model)
    {
        if (CutHitModel(
            device,
            closestVisual,
            &visualChangedTriangles,
            &visualFinalTriangles,
            &visualRadius,
            true))
        {
            visualCutModel = closestVisual.model;
            changedAny = true;
        }
    }

    int physicsChangedTriangles = 0;
    int physicsFinalTriangles = 0;
    float physicsRadius = 0.0f;
    bool physicsChanged = false;
    if (closestPhysics.entity != INVALID_ENTITY && closestPhysics.model)
    {
        if (closestPhysics.model != visualCutModel)
        {
            physicsChanged = CutHitModel(
                device,
                closestPhysics,
                &physicsChangedTriangles,
                &physicsFinalTriangles,
                &physicsRadius,
                false);
            changedAny = changedAny || physicsChanged;
        }
        else
        {
            physicsChanged = changedAny;
            physicsChangedTriangles = visualChangedTriangles;
            physicsFinalTriangles = visualFinalTriangles;
            physicsRadius = visualRadius;
        }

        if (physicsChanged)
        {
            if (auto* rb = registry.GetComponent<RigidBodyComponent>(closestPhysics.entity))
                PhysicsSystem::RefreshRigidBody(registry, closestPhysics.entity, rb);
        }
    }

    if (!changedAny)
    {
        OutputDebugStringA("[MeshCuttingTest] Ray hit meshes, but clipping did not change them.\n");
        return false;
    }

    if (closestVisual.entity != INVALID_ENTITY && closestVisual.model == closestPhysics.model)
    {
        if (auto* rb = registry.GetComponent<RigidBodyComponent>(closestVisual.entity))
            PhysicsSystem::RefreshRigidBody(registry, closestVisual.entity, rb);
    }

    const auto cutEnd = std::chrono::steady_clock::now();
    const double totalMs = std::chrono::duration<double, std::milli>(cutEnd - cutStart).count();

    char buffer[512];
    sprintf_s(buffer,
        "[MeshCuttingTest] Ray cut %.2f ms visualEntity=%u physicsEntity=%u visualChanged=%d visualFinal=%d physicsChanged=%d physicsFinal=%d visualRadius=%.2f physicsRadius=%.2f\n",
        totalMs,
        closestVisual.entity,
        closestPhysics.entity,
        visualChangedTriangles,
        visualFinalTriangles,
        physicsChangedTriangles,
        physicsFinalTriangles,
        visualRadius,
        physicsRadius);
    OutputDebugStringA(buffer);

    return true;
}
