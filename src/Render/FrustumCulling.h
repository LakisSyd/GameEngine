#pragma once

#include <DirectXCollision.h>
#include <DirectXMath.h>

// One instance is created per render pass. It converts each model's local AABB
// into world space and rejects objects that cannot appear in that pass.
class FrustumCulling
{
public:
    FrustumCulling(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection)
    {
        DirectX::BoundingFrustum viewFrustum;
        DirectX::BoundingFrustum::CreateFromMatrix(viewFrustum, projection);

        const DirectX::XMMATRIX inverseView = DirectX::XMMatrixInverse(nullptr, view);
        viewFrustum.Transform(m_worldFrustum, inverseView);
    }

    bool IsVisible(
        const DirectX::XMMATRIX& world,
        const DirectX::XMFLOAT3& localMin,
        const DirectX::XMFLOAT3& localMax,
        float padding = 0.0f) const
    {
        DirectX::BoundingBox localBounds;
        localBounds.Center = {
            (localMin.x + localMax.x) * 0.5f,
            (localMin.y + localMax.y) * 0.5f,
            (localMin.z + localMax.z) * 0.5f
        };
        localBounds.Extents = {
            (localMax.x - localMin.x) * 0.5f,
            (localMax.y - localMin.y) * 0.5f,
            (localMax.z - localMin.z) * 0.5f
        };
        localBounds.Extents.x += padding;
        localBounds.Extents.y += padding;
        localBounds.Extents.z += padding;

        DirectX::BoundingBox worldBounds;
        localBounds.Transform(worldBounds, world);
        return m_worldFrustum.Intersects(worldBounds);
    }

private:
    DirectX::BoundingFrustum m_worldFrustum;
};
