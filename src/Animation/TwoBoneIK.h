#pragma once
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>

// Model-space two-segment solver. Preserve segment lengths and use an optional
// pole target to choose the elbow bend plane (otherwise keep the authored one).
namespace TwoBoneIK
{
    inline bool Solve(DirectX::FXMVECTOR shoulder, DirectX::FXMVECTOR elbow,
        DirectX::FXMVECTOR wrist, DirectX::GXMVECTOR target,
        DirectX::XMVECTOR& solvedElbow, DirectX::XMVECTOR& solvedWrist,
        const DirectX::XMVECTOR* poleTarget = nullptr)
    {
        using namespace DirectX;
        const float a = XMVectorGetX(XMVector3Length(XMVectorSubtract(elbow, shoulder)));
        const float b = XMVectorGetX(XMVector3Length(XMVectorSubtract(wrist, elbow)));
        const XMVECTOR delta = XMVectorSubtract(target, shoulder);
        const float distance = XMVectorGetX(XMVector3Length(delta));
        if (a < 0.00001f || b < 0.00001f || distance < 0.00001f) return false;
        const XMVECTOR axis = XMVectorScale(delta, 1.0f / distance);
        const float d = std::clamp(distance, std::fabs(a - b) + 0.00001f, a + b - 0.00001f);
        XMVECTOR pole = poleTarget
            ? XMVectorSubtract(*poleTarget, shoulder)
            : XMVectorSubtract(elbow, shoulder);
        pole = XMVectorSubtract(pole, XMVectorMultiply(axis, XMVector3Dot(pole, axis)));
        if (XMVectorGetX(XMVector3LengthSq(pole)) < 0.000001f)
        {
            pole = XMVector3Cross(axis, XMVectorSet(0, 1, 0, 0));
            if (XMVectorGetX(XMVector3LengthSq(pole)) < 0.000001f)
                pole = XMVector3Cross(axis, XMVectorSet(1, 0, 0, 0));
        }
        const float along = (a*a + d*d - b*b) / (2*d);
        const float height = std::sqrt((std::max)(0.0f, a*a - along*along));
        solvedElbow = XMVectorAdd(XMVectorAdd(shoulder, XMVectorScale(axis, along)), XMVectorScale(XMVector3Normalize(pole), height));
        solvedWrist = XMVectorAdd(shoulder, XMVectorScale(axis, d));
        return true;
    }

    inline DirectX::XMMATRIX RotationBetween(DirectX::FXMVECTOR from, DirectX::FXMVECTOR to)
    {
        using namespace DirectX;
        const XMVECTOR f = XMVector3Normalize(from);
        const XMVECTOR t = XMVector3Normalize(to);
        const float dot = std::clamp(XMVectorGetX(XMVector3Dot(f, t)), -1.0f, 1.0f);
        if (dot > 0.999999f) return XMMatrixIdentity();
        XMVECTOR axis = XMVector3Cross(f, t);
        if (dot < -0.999999f)
        {
            axis = XMVector3Cross(f, XMVectorSet(0, 1, 0, 0));
            if (XMVectorGetX(XMVector3LengthSq(axis)) < 0.000001f)
                axis = XMVector3Cross(f, XMVectorSet(1, 0, 0, 0));
        }
        return XMMatrixRotationAxis(axis, std::acos(dot));
    }
}
