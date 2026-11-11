#pragma once

#include <DirectXMath.h>

// Дополняет обычный LightComponent направленным конусом света.
// Если компонента нет, источник продолжает работать как точечный.
struct SpotLightComponent
{
    // Направление из линзы в локальных координатах модели.
    // Blender/glTF lights face local -Z by convention. Imported ship ceiling
    // fixtures can override this (they face local -Y in SpaceShip.glb).
    DirectX::XMFLOAT3 localDirection = { 0.0f, 0.0f, -1.0f };

    // Внутри innerAngle свет полный, до outerAngle плавно затухает.
    float innerAngleDegrees = 22.0f;
    float outerAngleDegrees = 38.0f;
};
