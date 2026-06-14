#pragma once

#include <algorithm>
#include <DirectXMath.h>

#include "assimpmodelclass.h"

namespace ModelScaleUtils
{
    struct BoundsSize
    {
        float width = 0.0f;
        float height = 0.0f;
        float depth = 0.0f;
    };

    inline BoundsSize GetLocalSize(const AssimpModelClass& model)
    {
        DirectX::XMFLOAT3 minBounds;
        DirectX::XMFLOAT3 maxBounds;
        model.GetBoundingBox(&minBounds, &maxBounds);

        return {
            (std::max)(0.0f, maxBounds.x - minBounds.x),
            (std::max)(0.0f, maxBounds.y - minBounds.y),
            (std::max)(0.0f, maxBounds.z - minBounds.z)
        };
    }

    inline float UniformScaleForHeight(
        const AssimpModelClass& model,
        float targetHeight,
        float fallbackScale = 1.0f)
    {
        const float importedHeight = GetLocalSize(model).height;
        if (importedHeight <= 0.0001f)
            return fallbackScale;

        return targetHeight / importedHeight;
    }

    inline float UniformScaleToFit(
        const AssimpModelClass& model,
        float maxWidth,
        float maxHeight,
        float maxDepth,
        float fallbackScale = 1.0f)
    {
        const BoundsSize size = GetLocalSize(model);
        float scale = fallbackScale;

        if (size.width > 0.0001f)
            scale = (std::min)(scale, maxWidth / size.width);
        if (size.height > 0.0001f)
            scale = (std::min)(scale, maxHeight / size.height);
        if (size.depth > 0.0001f)
            scale = (std::min)(scale, maxDepth / size.depth);

        return scale;
    }
}
